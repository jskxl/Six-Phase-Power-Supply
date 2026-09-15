# -*- coding: utf-8 -*-
"""
RS485 (Modbus RTU) 显示板从机测试工具
用法:
  python rs485_test.py COM3                         # 读全部寄存器(默认地址1, 9600)
  python rs485_test.py COM3 --addr 2 --baud 19200   # 指定从机地址/波特率
  python rs485_test.py COM3 --write 0x0009 500      # 写CH1设置电压=5.00V
  python rs485_test.py COM3 --write 0x0011 1        # CH1输出ON
  python rs485_test.py COM3 --write 0x0011 0        # CH1输出OFF
依赖: pip install pyserial
接线: USB-485转换器 A/B 接显示板485的 A/B, GND共地
"""
import sys, argparse, time

try:
    import serial
except ImportError:
    print('缺少 pyserial, 请先执行: pip install pyserial')
    sys.exit(1)

BAUDS = [4800, 9600, 19200, 38400, 57600, 115200]

def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc >> 1) ^ 0xA001) if (crc & 1) else (crc >> 1)
    return crc

def frame(addr, func, payload):
    body = bytes([addr, func]) + bytes(payload)
    c = crc16(body)
    return body + bytes([c & 0xFF, (c >> 8) & 0xFF])

def read_regs(ser, addr):
    req = frame(addr, 0x03, [0x00, 0x00, 0x00, 0x17])   # 读 0x0000 起 23 个寄存器
    ser.reset_input_buffer()
    ser.write(req)
    resp = ser.read(4 + 46 + 2)
    if len(resp) < 5:
        return None
    if resp[0] != addr or resp[1] != 0x03:
        print('响应异常: ' + resp.hex(' '))
        return None
    n = resp[2]
    if len(resp) != 3 + n + 2:
        print('长度异常')
        return None
    c = crc16(resp[:-2])
    if (c & 0xFF) != resp[-2] or ((c >> 8) & 0xFF) != resp[-1]:
        print('CRC错误')
        return None
    data = resp[3:3 + n]
    return [(data[i] << 8) | data[i + 1] for i in range(0, n, 2)]

def show(regs):
    print('输入电压 : %.2fV' % (regs[0] / 100.0))
    for ch in range(4):
        out_v = regs[1 + ch] / 100.0
        out_i = regs[5 + ch] / 1000.0
        set_v = regs[9 + ch] / 100.0
        set_i = regs[13 + ch] / 1000.0
        ctl = 'ON' if regs[17 + ch] else 'OFF'
        print('CH%d 输出 %.2fV %.3fA | 设置 %.2fV %.3fA | %s' % (ch + 1, out_v, out_i, set_v, set_i, ctl))
    print('波特率寄存器: %d (%s)' % (regs[22], BAUDS[regs[22]] if regs[22] <= 5 else '?'))

def write_reg(ser, addr, reg, val):
    req = frame(addr, 0x06, [(reg >> 8) & 0xFF, reg & 0xFF, (val >> 8) & 0xFF, val & 0xFF])
    ser.reset_input_buffer()
    ser.write(req)
    resp = ser.read(8)
    if len(resp) < 8:
        print('无响应!')
        return False
    c = crc16(resp[:-2])
    ok = (c & 0xFF) == resp[-2] and ((c >> 8) & 0xFF) == resp[-1]
    if resp[:4] == req[:4] and ok:
        print('写入成功: 寄存器0x%04X = %d' % (reg, val))
        return True
    print('响应异常: ' + resp.hex(' '))
    return False

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('port', help='串口号, 如 COM3')
    ap.add_argument('--addr', type=int, default=1, help='从机地址(默认1)')
    ap.add_argument('--baud', type=int, default=9600, help='波特率(默认9600)')
    ap.add_argument('--write', nargs=2, metavar=('REG', 'VALUE'), help='写单个寄存器, 如 --write 0x0009 500')
    a = ap.parse_args()

    ser = serial.Serial(a.port, a.baud, timeout=0.5)
    print('连接 %s @%d 8N1, 从机地址=%d' % (a.port, a.baud, a.addr))

    if a.write:
        write_reg(ser, a.addr, int(a.write[0], 0), int(a.write[1], 0))
    else:
        for attempt in range(3):
            regs = read_regs(ser, a.addr)
            if regs is not None:
                show(regs)
                break
            print('第%d次无响应, 重试...' % (attempt + 1))
            time.sleep(0.2)
        else:
            print('3次均无响应! 检查: 485 A/B接线、共地、从机地址/波特率、DE/RE是否接PA8')
    ser.close()

if __name__ == '__main__':
    main()