# -*- coding: utf-8 -*-
"""
功率板模拟器 (按 显示板_功率板通讯协议 V1.0)
作用: 用 USB-TTL 模拟功率板, 与显示板联调:
  - 每 50ms 主动发送 0x81 测量帧 (seq/输入电压/6路输出电压电流/使能mask)
  - 收到显示板命令(0x01/0x02/0x03/0x04/0x06) 更新状态并回 0x82 ACK
  - 收到 0x05 查询立即回一帧 0x81
  - 模拟闭环: 输出向设定值逐步收敛(约5帧到位)
用法:
  python sim_power.py COM3               # 默认 50ms 周期, 输入电压 24.00V
  python sim_power.py COM3 --interval 50 --vin 24.0
依赖: pip install pyserial
接线: 适配器TX -> 显示板 PB11(RX), 适配器RX -> 显示板 PB10(TX), GND 共地
"""
import sys, time, argparse

try:
    import serial
except ImportError:
    print('缺少 pyserial, 请先执行: pip install pyserial')
    sys.exit(1)

LEN = {0x01: 6, 0x02: 1, 0x03: 3, 0x04: 3, 0x05: 0, 0x06: 24}
FUNC_NAME = {0x01: '设置单通道', 0x02: '设置使能掩码', 0x03: '设置电压',
             0x04: '设置电流', 0x05: '查询', 0x06: '设置全部通道'}


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


def frame(func, data):
    body = bytes([func]) + bytes(data)
    crc = crc16(body)
    return b'\xAA\x55' + body + bytes([crc & 0xFF, (crc >> 8) & 0xFF])


class PowerBoard:
    def __init__(self, vin=24.0):
        self.v_set = [0] * 6
        self.i_set = [0] * 6
        self.out_v = [0] * 6
        self.out_i = [0] * 6
        self.en = [0] * 6
        self.in_v = int(vin * 100)
        self.seq = 0

    def meas_frame(self):
        # 模拟闭环: 输出逐步收敛到设定值(无论使能与否, 便于联调观察; mask单独表示ON/OFF)
        for i in range(6):
            self.out_v[i] += int((self.v_set[i] - self.out_v[i]) * 0.3)
            self.out_i[i] += int((self.i_set[i] - self.out_i[i]) * 0.3)
        d = bytearray(28)
        d[0] = self.seq
        d[1] = (self.in_v >> 8) & 0xFF
        d[2] = self.in_v & 0xFF
        for i in range(6):
            d[3 + i * 2] = (self.out_v[i] >> 8) & 0xFF
            d[4 + i * 2] = self.out_v[i] & 0xFF
            d[15 + i * 2] = (self.out_i[i] >> 8) & 0xFF
            d[16 + i * 2] = self.out_i[i] & 0xFF
        mask = 0
        for i in range(6):
            if self.en[i]:
                mask |= (1 << i)
        d[27] = mask
        self.seq = (self.seq + 1) & 0xFF
        return frame(0x81, d)

    def handle(self, func, data):
        """处理显示板命令, 返回需要回复的帧(可能为空)"""
        if func == 0x01:                      # 设置单通道 电压+电流+使能
            ch = data[0] - 1
            if not (0 <= ch < 6):
                return frame(0x82, [0x01, 1])
            self.v_set[ch] = (data[1] << 8) | data[2]
            self.i_set[ch] = (data[3] << 8) | data[4]
            self.en[ch] = 1 if data[5] else 0
            print('<< 0x01 CH%d 电压=%.2fV 电流=%.3fA %s' % (
                ch + 1, self.v_set[ch] / 100.0, self.i_set[ch] / 1000.0,
                'ON' if self.en[ch] else 'OFF'))
            return frame(0x82, [0x01, 0])
        if func == 0x02:                      # 设置使能掩码
            mask = data[0]
            for i in range(6):
                self.en[i] = 1 if (mask >> i) & 1 else 0
            print('<< 0x02 mask=0x%02X %s' % (mask,
                  ','.join('CH%d' % (i + 1) for i in range(6) if self.en[i]) or '全关'))
            return frame(0x82, [0x02, 0])
        if func == 0x03:                      # 设置单通道电压
            ch = data[0] - 1
            if not (0 <= ch < 6):
                return frame(0x82, [0x03, 1])
            self.v_set[ch] = (data[1] << 8) | data[2]
            print('<< 0x03 CH%d 电压=%.2fV' % (ch + 1, self.v_set[ch] / 100.0))
            return frame(0x82, [0x03, 0])
        if func == 0x04:                      # 设置单通道电流
            ch = data[0] - 1
            if not (0 <= ch < 6):
                return frame(0x82, [0x04, 1])
            self.i_set[ch] = (data[1] << 8) | data[2]
            print('<< 0x04 CH%d 电流=%.3fA' % (ch + 1, self.i_set[ch] / 1000.0))
            return frame(0x82, [0x04, 0])
        if func == 0x05:                      # 查询 -> 立即回一帧测量
            print('<< 0x05 查询')
            return self.meas_frame()
        if func == 0x06:                      # 设置全部通道电压+电流
            for i in range(6):
                self.v_set[i] = (data[i * 2] << 8) | data[i * 2 + 1]
                self.i_set[i] = (data[12 + i * 2] << 8) | data[12 + i * 2 + 1]
            print('<< 0x06 全部: V=' + ' '.join('%.2f' % (self.v_set[i] / 100.0) for i in range(6)) +
                  ' I=' + ' '.join('%.3f' % (self.i_set[i] / 1000.0) for i in range(6)))
            return frame(0x82, [0x06, 0])
        return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('port', help='串口号, 如 COM3')
    ap.add_argument('--interval', type=int, default=50, help='测量帧周期ms(默认50)')
    ap.add_argument('--vin', type=float, default=24.0, help='模拟输入电压V(默认24.00)')
    args = ap.parse_args()

    ser = serial.Serial(args.port, 115200, timeout=0)
    pb = PowerBoard(args.vin)
    buf = bytearray()
    last_meas = time.monotonic()
    last_summary = time.monotonic()
    n_meas = 0
    n_cmd = 0
    n_crc = 0

    print('功率板模拟器启动: %s @115200 8N1, 周期%dms, 输入%.2fV' % (
        args.port, args.interval, args.vin))
    print('Ctrl+C 停止')

    while True:
        raw = ser.read(ser.in_waiting or 1)
        for b in raw:
            buf.append(b)
            if len(buf) >= 3:
                func = buf[2]
                n = LEN.get(func)
                if n is None:
                    print('!! 非法功能码 0x%02X' % func)
                    buf.clear()
                    continue
                need = 1 + n + 2
                if len(buf) - 2 >= need:
                    body = bytes(buf[2:2 + need])
                    del buf[:2 + need]
                    got = body[-2] | (body[-1] << 8)
                    if crc16(body[:-2]) != got:
                        n_crc += 1
                        print('!! CRC错误, 丢弃')
                        continue
                    n_cmd += 1
                    reply = pb.handle(body[0], body[1:1 + n])
                    if reply:
                        ser.write(reply)

        now = time.monotonic()
        if now - last_meas >= args.interval / 1000.0:
            last_meas = now
            ser.write(pb.meas_frame())
            n_meas += 1
        if now - last_summary >= 1.0:
            last_summary = now
            mask = sum((1 << i) for i in range(6) if pb.en[i])
            print('>> 已发%d帧测量 seq=%d mask=0x%02X | 收到%d命令 CRC错%d | 示例 CH1: 设定%.2fV/%.3fA 输出%.2fV/%.3fA' % (
                n_meas, pb.seq - 1, mask, n_cmd, n_crc,
                pb.v_set[0] / 100.0, pb.i_set[0] / 1000.0,
                pb.out_v[0] / 100.0, pb.out_i[0] / 1000.0))
        time.sleep(0.001)


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print('\n模拟器已停止')
    except serial.SerialException as e:
        print('串口打开失败: %s' % e)