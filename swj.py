import sys
import time
import threading
import queue

import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print('缺少 pyserial, 请先执行: pip install pyserial')
    sys.exit(1)

REG_IN_V  = 0x0000
REG_OUT_V = 0x0001
REG_OUT_I = 0x0007
REG_SET_V = 0x000D
REG_SET_I = 0x0013
REG_CTRL  = 0x0019
REG_CAL   = 0x001F
REG_BAUD  = 0x0020

POLL_START = 0x0000
POLL_COUNT = 33
BAUDS = [4800, 9600, 19200, 38400, 57600, 115200]


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


def frame_read(addr, start, count):
    body = bytes([addr, 0x03, (start >> 8) & 0xFF, start & 0xFF,
                  (count >> 8) & 0xFF, count & 0xFF])
    crc = crc16(body)
    return body + bytes([crc & 0xFF, crc >> 8])


def frame_write_single(addr, reg, val):
    body = bytes([addr, 0x06, (reg >> 8) & 0xFF, reg & 0xFF,
                  (val >> 8) & 0xFF, val & 0xFF])
    crc = crc16(body)
    return body + bytes([crc & 0xFF, crc >> 8])


def frame_write_multi(addr, start, values):
    count = len(values)
    body = bytearray([addr, 0x10, (start >> 8) & 0xFF, start & 0xFF,
                      (count >> 8) & 0xFF, count & 0xFF, count * 2])
    for v in values:
        body += bytes([(v >> 8) & 0xFF, v & 0xFF])
    crc = crc16(bytes(body))
    return bytes(body) + bytes([crc & 0xFF, crc >> 8])


def scan_slaves(port, bauds, addrs, per_timeout=0.05):
    found = []
    ser = serial.Serial(port, bauds[0], timeout=per_timeout)
    try:
        for baud in bauds:
            ser.baudrate = baud
            for addr in addrs:
                ser.reset_input_buffer()
                ser.write(frame_read(addr, 0x0000, 1))
                buf = bytearray()
                deadline = time.monotonic() + per_timeout
                while time.monotonic() < deadline:
                    chunk = ser.read(ser.in_waiting or 1)
                    if chunk:
                        buf.extend(chunk)
                        if len(buf) >= 7 and buf[0] == addr and buf[1] == 0x03:
                            bcnt = buf[2]
                            if len(buf) >= 3 + bcnt + 2:
                                break
                if len(buf) >= 7 and buf[0] == addr and buf[1] == 0x03:
                    bcnt = buf[2]
                    if len(buf) >= 3 + bcnt + 2:
                        fr = bytes(buf[:3 + bcnt + 2])
                        if crc16(fr[:-2]) == ((fr[-1] << 8) | fr[-2]):
                            found.append((addr, baud))
    finally:
        ser.close()
    return found


class ModbusMaster:
    def __init__(self, port, baud, addr, evt_queue):
        self.ser = serial.Serial(port, baud, timeout=0.1)
        self.addr = addr
        self.evt = evt_queue
        self.lock = threading.Lock()
        self.stop = False
        self.rxbuf = bytearray()
        self.rx_thread = threading.Thread(target=self._rx_loop, daemon=True)
        self.rx_thread.start()

    def _rx_loop(self):
        while not self.stop:
            try:
                if self.ser.in_waiting:
                    data = self.ser.read(self.ser.in_waiting)
                    self.evt.put(('RAW', data.hex().upper()))
                    self._feed(data)
                else:
                    time.sleep(0.02)
            except Exception:
                break

    def _feed(self, data):
        self.rxbuf.extend(data)
        while True:
            n = len(self.rxbuf)
            i = 0
            while i < n and self.rxbuf[i] != self.addr:
                i += 1
            if i > 0:
                del self.rxbuf[:i]
                n = len(self.rxbuf)
            if n < 2:
                return
            func = self.rxbuf[1]
            if func & 0x80:
                flen = 5
            elif func == 0x03:
                if n < 3:
                    return
                flen = 3 + self.rxbuf[2] + 2
            elif func in (0x06, 0x10):
                flen = 8
            else:
                del self.rxbuf[0]
                continue
            if n < flen:
                return
            frame = bytes(self.rxbuf[:flen])
            del self.rxbuf[:flen]
            self._handle_frame(frame)

    def _handle_frame(self, frame):
        if len(frame) < 4:
            return
        crc_rx = (frame[-1] << 8) | frame[-2]
        if crc16(frame[:-2]) != crc_rx:
            self.evt.put(('LOG', 'CRC错误: ' + frame.hex().upper()))
            return
        func = frame[1]
        if func & 0x80:
            self.evt.put(('LOG', '异常应答 func=0x%02X code=%d' % (func & 0x7F, frame[2])))
            return
        if func == 0x03:
            bcnt = frame[2]
            regs = []
            for k in range(bcnt // 2):
                regs.append((frame[3 + k * 2] << 8) | frame[4 + k * 2])
            self.evt.put(('READ', regs))
        elif func == 0x06:
            reg = (frame[2] << 8) | frame[3]
            val = (frame[4] << 8) | frame[5]
            self.evt.put(('LOG', 'ACK 写单个 reg=0x%04X val=%d' % (reg, val)))
            if reg == REG_BAUD:
                self.evt.put(('BAUD', val))
        elif func == 0x10:
            start = (frame[2] << 8) | frame[3]
            count = (frame[4] << 8) | frame[5]
            self.evt.put(('LOG', 'ACK 写多个 start=0x%04X count=%d' % (start, count)))

    def send(self, data):
        with self.lock:
            self.ser.write(data)

    def read_regs(self):
        self.send(frame_read(self.addr, POLL_START, POLL_COUNT))

    def write_single(self, reg, val):
        self.send(frame_write_single(self.addr, reg, val))

    def write_multi(self, start, values):
        self.send(frame_write_multi(self.addr, start, values))

    def set_baud(self, baud):
        self.ser.baudrate = baud

    def close(self):
        self.stop = True
        try:
            self.ser.close()
        except Exception:
            pass


CH_NUM = 6


class PlcSimGUI:
    def __init__(self, root):
        self.root = root
        self.root.title('PLC 模拟上位机 - 显示板 RS485 (新固件 6通道+扩展)')
        self.root.geometry('920x700')
        self.root.minsize(840, 620)

        self.master = None
        self.polling = False
        self.tx_busy = False
        self.baud_switch_pending = False
        self.evt = queue.Queue()
        self.raw_on = tk.BooleanVar(value=False)
        self._build_ui()
        self._refresh_ports()
        self.root.after(80, self._poll_queue)
        self.root.protocol('WM_DELETE_WINDOW', self._on_close)

    def _build_ui(self):
        top = ttk.LabelFrame(self.root, text='串口连接 (Modbus RTU 主机)')
        top.pack(fill='x', padx=8, pady=(8, 4))

        ttk.Label(top, text='端口:').pack(side='left', padx=(8, 2))
        self.port_cb = ttk.Combobox(top, width=14, state='readonly')
        self.port_cb.pack(side='left', padx=2)
        ttk.Label(top, text='波特率:').pack(side='left', padx=(10, 2))
        self.baud_cb = ttk.Combobox(top, width=8, state='readonly',
                                    values=['4800', '9600', '19200', '38400', '57600', '115200'])
        self.baud_cb.set('9600')
        self.baud_cb.pack(side='left', padx=2)
        ttk.Label(top, text='从机地址:').pack(side='left', padx=(10, 2))
        self.addr_spin = ttk.Spinbox(top, from_=1, to=31, width=4)
        self.addr_spin.set(1)
        self.addr_spin.pack(side='left', padx=2)
        self.conn_btn = ttk.Button(top, text='连接', width=8, command=self._toggle_conn)
        self.conn_btn.pack(side='left', padx=(14, 4))
        self.status_lbl = ttk.Label(top, text='未连接', foreground='gray')
        self.status_lbl.pack(side='left', padx=8)
        self.poll_chk = ttk.Checkbutton(top, text='周期读实时数据', command=self._toggle_poll)
        self.poll_chk.pack(side='right', padx=8)
        ttk.Button(top, text='刷新端口', command=self._refresh_ports).pack(side='right', padx=4)
        self.scan_btn = ttk.Button(top, text='扫描从机', command=self._cmd_scan)
        self.scan_btn.pack(side='right', padx=4)

        mid = ttk.Frame(self.root)
        mid.pack(fill='both', expand=True, padx=8, pady=4)

        left = ttk.LabelFrame(mid, text='实时数据 (0x03 读33寄存器)')
        left.pack(side='left', fill='both', expand=True, padx=(0, 4))
        self.vin_lbl = ttk.Label(left, text='输入电压: --.--V', font=('Microsoft YaHei', 11, 'bold'))
        self.vin_lbl.pack(anchor='w', padx=10, pady=(6, 2))
        self.ch_lbls = []
        for i in range(CH_NUM):
            lbl = ttk.Label(left, text='CH%d  --.--V  --.---A  --  设:--.--V/--.---A' % (i + 1),
                            font=('Consolas', 11))
            lbl.pack(anchor='w', padx=14, pady=2)
            self.ch_lbls.append(lbl)
        self.baud_lbl = ttk.Label(left, text='从机波特率: --', font=('Consolas', 10), foreground='orange')
        self.baud_lbl.pack(anchor='w', padx=14, pady=(4, 0))

        right = ttk.LabelFrame(mid, text='控制 (0x06/0x10 写)')
        right.pack(side='left', fill='y', padx=(4, 0))

        self.i_ents = []
        self.ctrl_lbls = []
        for i in range(CH_NUM):
            row = ttk.Frame(right)
            row.pack(fill='x', padx=6, pady=1)
            ttk.Label(row, text='CH%d' % (i + 1), width=4).pack(side='left')
            ttk.Label(row, text='A').pack(side='left')
            ie = ttk.Entry(row, width=6); ie.insert(0, '1.00'); ie.pack(side='left', padx=2)
            ttk.Button(row, text='设电流', width=7, command=lambda c=i: self._cmd_set_i(c)).pack(side='left', padx=2)
            ttk.Button(row, text='ON', width=4, command=lambda c=i: self._cmd_enable(c, 1)).pack(side='left', padx=1)
            ttk.Button(row, text='OFF', width=4, command=lambda c=i: self._cmd_enable(c, 0)).pack(side='left', padx=1)
            st = ttk.Label(row, text='--', width=4, foreground='gray')
            st.pack(side='left', padx=2)
            self.i_ents.append(ie); self.ctrl_lbls.append(st)

        ttk.Button(right, text='批量下发6通道电流(0x10)',
                   command=self._cmd_set_all).pack(fill='x', padx=6, pady=3)

        ops = ttk.LabelFrame(right, text='扩展功能')
        ops.pack(fill='x', padx=6, pady=(2, 4))
        ttk.Button(ops, text='电流清0 (0x001F)', command=self._cmd_cal).pack(fill='x', padx=6, pady=2)
        r1 = ttk.Frame(ops); r1.pack(fill='x', padx=6, pady=1)
        ttk.Label(r1, text='新波特率:').pack(side='left')
        self.newbaud_cb = ttk.Combobox(r1, width=6, state='readonly',
                                       values=['4800', '9600', '19200', '38400', '57600', '115200'])
        self.newbaud_cb.set('19200')
        self.newbaud_cb.pack(side='left', padx=2)
        ttk.Button(r1, text='下发', width=6, command=self._cmd_set_baud).pack(side='left', padx=2)
        r2 = ttk.Frame(ops); r2.pack(fill='x', padx=6, pady=1)
        ttk.Label(r2, text='新地址:').pack(side='left')
        self.newaddr = ttk.Entry(r2, width=4); self.newaddr.insert(0, '2'); self.newaddr.pack(side='left', padx=2)
        ttk.Button(r2, text='广播改地址', command=self._cmd_change_addr).pack(side='left', padx=2)

        bottom = ttk.LabelFrame(self.root, text='通信日志')
        bottom.pack(fill='both', expand=True, padx=8, pady=(4, 8))
        self.log_txt = scrolledtext.ScrolledText(bottom, height=12, font=('Consolas', 9))
        self.log_txt.pack(fill='both', expand=True, padx=4, pady=4)
        self.log_txt.configure(state='disabled')

        rawf = ttk.LabelFrame(self.root, text='原始数据 (接收hex, 判断乱码)')
        rawf.pack(fill='both', expand=True, padx=8, pady=(0, 8))
        topbar = ttk.Frame(rawf); topbar.pack(fill='x', padx=4, pady=2)
        self.raw_chk = ttk.Checkbutton(topbar, text='记录原始数据', variable=self.raw_on)
        self.raw_chk.pack(side='left')
        self.raw_txt = scrolledtext.ScrolledText(rawf, height=8, font=('Consolas', 8))
        self.raw_txt.pack(fill='both', expand=True, padx=4, pady=(0, 4))
        self.raw_txt.configure(state='disabled')

    def _refresh_ports(self):
        try:
            ports = [p.device for p in list_ports.comports()]
        except Exception:
            ports = []
        self.port_cb['values'] = ports
        if ports and not self.port_cb.get():
            self.port_cb.set(ports[0])

    def _toggle_conn(self):
        if self.master:
            self._disconnect()
        else:
            self._connect()

    def _connect(self):
        port = self.port_cb.get()
        baud = int(self.baud_cb.get())
        addr = int(self.addr_spin.get())
        if not port:
            messagebox.showwarning('提示', '请选择串口')
            return
        try:
            self.master = ModbusMaster(port, baud, addr, self.evt)
        except Exception as e:
            messagebox.showerror('连接失败', str(e))
            return
        self.conn_btn.config(text='断开')
        self.status_lbl.config(text='已连接 %s @ %d 从机=%d' % (port, baud, addr), foreground='green')
        self.log('连接 %s @ %d, 从机地址 %d' % (port, baud, addr))

    def _disconnect(self):
        if self.master:
            self.master.close()
            self.master = None
        self.conn_btn.config(text='连接')
        self.status_lbl.config(text='未连接', foreground='gray')
        self.log('已断开')

    def _cmd_scan(self):
        port = self.port_cb.get()
        if not port:
            messagebox.showwarning('提示', '请先选择串口')
            return
        if self.master:
            self._disconnect()
        self.scan_btn.config(state='disabled')
        threading.Thread(target=self._scan_run, args=(port,), daemon=True).start()

    def _scan_run(self, port):
        bauds = [int(x) for x in self.baud_cb['values']]
        cur = int(self.baud_cb.get())
        addrs = list(range(1, 32))
        self.evt.put(('LOG', '===== 扫描从机开始: %s @ %d (地址1~31) =====' % (port, cur)))
        found = scan_slaves(port, [cur], addrs)
        if not found:
            self.evt.put(('LOG', '当前波特率 %d 未找到, 继续扫描全部波特率...' % cur))
            found = scan_slaves(port, bauds, addrs)
        if found:
            txt = '  '.join('地址%d @ %d' % (a, b) for a, b in found)
            self.evt.put(('LOG', '===== 发现从机: %s =====' % txt))
        else:
            self.evt.put(('LOG', '===== 未发现任何从机 ====='))
            self.evt.put(('LOG', '请检查: 485 A/B 是否接反、是否共地、DE/RE 是否接 PA8、板子供电与固件'))
        self.evt.put(('SCAN', found))

    def _on_scan(self, found):
        self.scan_btn.config(state='normal')
        if found:
            addr, baud = found[0]
            self.addr_spin.set(addr)
            self.baud_cb.set(str(baud))
            self.log('已自动选择 地址%d @ %d, 点击「连接」开始通讯' % (addr, baud))
        else:
            self.log('扫描完成: 无响应。确认接线/供电后重试。')

    def _toggle_poll(self):
        if not self.master:
            self.poll_chk.deselect()
            messagebox.showwarning('提示', '请先连接')
            return
        self.polling = not self.polling

    def _poll_queue(self):
        try:
            while True:
                evt = self.evt.get_nowait()
                if evt[0] == 'LOG':
                    self.log(evt[1])
                elif evt[0] == 'READ':
                    self._update_read(evt[1])
                elif evt[0] == 'BAUD':
                    self._on_baud_ack(evt[1])
                elif evt[0] == 'RAW':
                    self._show_raw(evt[1])
                elif evt[0] == 'SCAN':
                    self._on_scan(evt[1])
        except queue.Empty:
            pass
        self.root.after(80, self._poll_queue)

    def _update_read(self, regs):
        if len(regs) < 33:
            self.log('读回寄存器数不足: %d (检查链路接线, 应回33个)' % len(regs))
            return
        vin = regs[0] / 100.0
        self.vin_lbl.config(text='输入电压: %.2fV' % vin)
        for i in range(CH_NUM):
            v = regs[1 + i] / 100.0
            a = regs[7 + i] / 1000.0
            sv = regs[13 + i] / 100.0
            si = regs[19 + i] / 1000.0
            ctrl = regs[25 + i]
            st = {0: 'OFF', 1: 'ON'}.get(ctrl, '--')
            self.ch_lbls[i].config(
                text='CH%d  %6.2fV  %7.3fA  [%s]  设:%5.2fV/%5.3fA' % (i + 1, v, a, st, sv, si))
            self.ctrl_lbls[i].config(text=st,
                                     foreground='green' if ctrl == 1 else 'gray')
        baud_idx = regs[32]
        if baud_idx <= 5:
            self.baud_lbl.config(text='从机波特率: %d' % BAUDS[baud_idx])

    def log(self, msg):
        self.log_txt.configure(state='normal')
        self.log_txt.insert('end', '[%s] %s\n' % (time.strftime('%H:%M:%S'), msg))
        self.log_txt.see('end')
        self.log_txt.configure(state='disabled')

    def _show_raw(self, hexs):
        if not self.raw_on.get():
            return
        self.raw_txt.configure(state='normal')
        self.raw_txt.insert('end', hexs + '\n')
        lines = int(self.raw_txt.index('end-1c').split('.')[0])
        if lines > 200:
            self.raw_txt.delete('1.0', '%d.0' % (lines - 100))
        self.raw_txt.configure(state='disabled')

    def _check(self):
        if not self.master:
            messagebox.showwarning('提示', '请先连接')
            return False
        return True

    def _busy_clear(self):
        self.tx_busy = False


    def _cmd_set_i(self, ch):
        if not self._check():
            return
        try:
            a = float(self.i_ents[ch].get())
            if not (0 <= a <= 1.5):
                messagebox.showwarning('提示', '电流 0~1.5A')
                return
        except ValueError:
            messagebox.showwarning('提示', '请输入数字')
            return
        raw = int(round(a * 1000))
        self.master.write_single(REG_SET_I + ch, raw)
        self.log('写设置电流 CH%d = %.3fA (0x%04X=%d)' % (ch + 1, a, REG_SET_I + ch, raw))

    def _cmd_enable(self, ch, val):
        if not self._check():
            return
        self.master.write_single(REG_CTRL + ch, val)
        self.log('写输出控制 CH%d = %d' % (ch + 1, val))


    def _cmd_set_all(self):
        if not self._check():
            return
        try:
            ii = []
            for i in range(CH_NUM):
                a = float(self.i_ents[i].get())
                if not (0 <= a <= 1.5):
                    raise ValueError
                ii.append(int(round(a * 1000)))
        except ValueError:
            messagebox.showwarning('提示', '请输入有效电流 0~1.5A')
            return
        self.master.write_multi(REG_SET_I, ii)
        self.log('批量下发 6 通道电流')

    def _cmd_cal(self):
        if not self._check():
            return
        self.master.write_single(REG_CAL, 1)
        self.log('触发电流清0 (0x001F=1)')

    def _cmd_set_baud(self):
        if not self._check():
            return
        try:
            idx = BAUDS.index(int(self.newbaud_cb.get()))
        except ValueError:
            messagebox.showwarning('提示', '选择波特率')
            return
        self.master.write_single(REG_BAUD, idx)
        self.log('写波特率设置 0x%04X=%d (%d)' % (REG_BAUD, idx, BAUDS[idx]))
        self.baud_switch_pending = BAUDS[idx]

    def _on_baud_ack(self, idx):
        if idx > 5:
            return
        if self.baud_switch_pending:
            new_baud = self.baud_switch_pending
            self.baud_switch_pending = False
            self.root.after(300, lambda: self._switch_baud(new_baud))

    def _switch_baud(self, baud):
        if not self.master:
            return
        try:
            self.master.set_baud(baud)
            self.baud_cb.set(str(baud))
            self.log('上位机已切换到 %d, 从机100ms后生效' % baud)
        except Exception as e:
            self.log('切换波特率失败: %s' % e)

    def _cmd_change_addr(self):
        if not self._check():
            return
        try:
            a = int(self.newaddr.get())
            if not (1 <= a <= 31):
                messagebox.showwarning('提示', '地址 1~31')
                return
        except ValueError:
            messagebox.showwarning('提示', '请输入数字')
            return
        body = bytes([0x00, 0x06, 0x00, 0x00, (a >> 8) & 0xFF, a & 0xFF])
        crc = crc16(body)
        self.master.send(body + bytes([crc & 0xFF, crc >> 8]))
        self.log('广播改地址 -> %d' % a)
        self.master.addr = a
        self.addr_spin.set(a)

    def _poll_tick(self):
        if self.master and self.polling and not self.tx_busy:
            self.master.read_regs()
        self.root.after(500, self._poll_tick)

    def _on_close(self):
        self._disconnect()
        self.root.destroy()


def main():
    root = tk.Tk()
    app = PlcSimGUI(root)
    root.after(500, app._poll_tick)
    root.mainloop()


if __name__ == '__main__':
    main()
