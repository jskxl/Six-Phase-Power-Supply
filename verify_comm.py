# -*- coding: utf-8 -*-
"""
显示板 <-> 功率板 通讯抓帧校验工具 (按 显示板_功率板通讯协议 V1.0)
用法:
  python verify_comm.py COM3          # 监听并自动解析/校验
  python verify_comm.py COM3 --hex    # 只输出原始HEX
依赖: pip install pyserial
接线(USB-TTL 3.3V):
  想看功率板发给显示板: 适配器RX 接 显示板 PB11 (显示板收), GND共地, TX悬空
  想看显示板发给功率板: 适配器RX 接 显示板 PB10 (显示板发), GND共地, TX悬空
"""
import sys, time

try:
    import serial
except ImportError:
    print('缺少 pyserial, 请先执行: pip install pyserial')
    sys.exit(1)

PORT = sys.argv[1] if len(sys.argv) > 1 else 'COM3'
HEX_MODE = '--hex' in sys.argv
BAUD = 115200

LEN = {0x01: 6, 0x02: 1, 0x03: 3, 0x04: 3, 0x05: 0, 0x06: 24,
       0x81: 28, 0x82: 2, 0x83: 2}
FUNC_NAME = {0x01: '设置单通道', 0x02: '设置使能掩码', 0x03: '设置电压',
             0x04: '设置电流', 0x05: '查询', 0x06: '设置全部通道',
             0x81: '测量上报', 0x82: 'ACK', 0x83: '报警'}


def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc


def show_frame(func, data):
    if func == 0x81:
        seq = data[0]
        in_v = (data[1] << 8) | data[2]
        v = [(data[3 + i * 2] << 8) | data[4 + i * 2] for i in range(6)]
        c = [(data[15 + i * 2] << 8) | data[16 + i * 2] for i in range(6)]
        mask = data[27]
        on = ','.join('CH%d' % (i + 1) for i in range(6) if (mask >> i) & 1) or '全关'
        print('81测量 seq=%d 输入=%.2fV mask=0x%02X(%s)' % (seq, in_v / 100.0, mask, on))
        print('   电压: ' + ' '.join('CH%d=%.2fV' % (i + 1, v[i] / 100.0) for i in range(6)))
        print('   电流: ' + ' '.join('CH%d=%.3fA' % (i + 1, c[i] / 1000.0) for i in range(6)))
    elif func == 0x82:
        st = {0: 'OK', 1: '通道号非法', 2: '数据超范围', 3: '校验错', 4: '执行失败'}.get(data[1], '?')
        print('82ACK cmd=0x%02X status=0x%02X(%s)' % (data[0], data[1], st))
    elif func == 0x83:
        print('83报警 alarm=0x%02X fault=0x%02X' % (data[0], data[1]))
    else:
        print('%02X %s %s' % (func, FUNC_NAME.get(func, '未知'), data.hex(' ').upper()))


def main():
    ser = serial.Serial(PORT, BAUD, timeout=0.2)
    print('已打开 %s @115200 8N1, Ctrl+C 停止' % PORT)
    buf = bytearray()
    total = 0
    crc_bad = 0
    t0 = time.time()
    t_last = t0

    while True:
        ch = ser.read(1)
        if not ch:
            continue
        b = ch[0]

        if len(buf) == 0:                 # 等帧头 AA
            if b == 0xAA:
                buf.append(b)
            continue
        if len(buf) == 1:                 # 等 55
            if b == 0x55:
                buf.append(b)
            elif b == 0xAA:
                pass                      # 连续 AA
            else:
                buf.clear()
            continue
        buf.append(b)

        if len(buf) >= 3:
            func = buf[2]
            n = LEN.get(func)
            if n is None:
                print('!! 非法功能码 0x%02X, 重新同步' % func)
                buf.clear()
                continue
            need = 1 + n + 2               # func + data + crc
            if len(buf) - 2 >= need:
                frame = bytes(buf[2:2 + need])
                del buf[:2 + need]         # 保留可能残留的下一帧字节
                got = frame[-2] | (frame[-1] << 8)
                calc = crc16(frame[:-2])
                if got != calc:
                    crc_bad += 1
                    print('!! CRC错误 func=0x%02X 收到=0x%04X 计算=0x%04X' % (frame[0], got, calc))
                    continue
                total += 1
                if HEX_MODE:
                    print('AA 55 ' + frame.hex(' ').upper())
                else:
                    show_frame(frame[0], frame[1:1 + n])
                now = time.time()
                if now - t_last >= 1.0:
                    print('--- 共%d帧 平均%.1f帧/秒 CRC错误%d ---' % (total, total / (now - t0), crc_bad))
                    t_last = now


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print('\n已停止')
    except serial.SerialException as e:
        print('串口打开失败: %s' % e)