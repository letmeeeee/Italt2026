#!/usr/bin/env python3
import os, io, csv, json, time, sys, subprocess, threading, socket, re
import ipaddress
import numpy as np
import pymysql
import sqlite3
import configparser
from typing import Optional, Tuple
from pathlib import Path
from datetime import datetime, timedelta
from flask import Flask, render_template, request, jsonify, make_response, Response
from flask_socketio import SocketIO
from sqlalchemy import create_engine, text
from sqlalchemy.exc import SQLAlchemyError
from pymodbus.client import ModbusTcpClient
# =========================
# USB 启用/禁用控制接口
# =========================

USB_HUB_AUTH_PATH = "/sys/bus/usb/devices/3-1/authorized"

# ================== 工具函数 ==================
def decode_proc_mount_path(path):
    """
    /proc/mounts 中空格会显示为 \\040，这里做简单还原
    """
    return path.replace("\\040", " ")


def get_mounted_usb_partitions():
    """
    获取当前已经挂载的 U 盘分区。
    这里主要识别 /dev/sdXN，例如：
        /dev/sda1
        /dev/sdb1
    """
    mounted_list = []

    try:
        with open("/proc/mounts", "r") as f:
            for line in f:
                parts = line.split()
                if len(parts) < 2:
                    continue

                dev = parts[0]
                mount_point = decode_proc_mount_path(parts[1])

                # 常见 U 盘块设备：/dev/sda1、/dev/sdb1
                if dev.startswith("/dev/sd") and dev[-1].isdigit():
                    mounted_list.append({
                        "dev": dev,
                        "mount_point": mount_point
                    })

    except Exception as e:
        return False, str(e), []

    return True, "", mounted_list


def safe_umount_usb_partitions():
    """
    禁用 USB 前先 sync + umount，避免 U 盘文件系统损坏。
    """
    ok, err, mounted_list = get_mounted_usb_partitions()
    if not ok:
        return False, f"读取挂载信息失败: {err}", []

    # 先同步缓存
    try:
        subprocess.run(["sync"], timeout=5)
    except Exception as e:
        return False, f"sync 执行失败: {e}", mounted_list

    errors = []

    for item in mounted_list:
        mount_point = item["mount_point"]

        try:
            result = subprocess.run(
                ["umount", mount_point],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                timeout=10
            )

            if result.returncode != 0:
                errors.append({
                    "dev": item["dev"],
                    "mount_point": mount_point,
                    "error": result.stderr.strip()
                })

        except Exception as e:
            errors.append({
                "dev": item["dev"],
                "mount_point": mount_point,
                "error": str(e)
            })

    if errors:
        return False, "部分 USB 分区卸载失败", errors

    return True, "", mounted_list


def read_usb_status():
    """
    读取 USB Hub 当前状态。
    authorized = 1 表示启用
    authorized = 0 表示禁用
    """
    if not os.path.exists(USB_HUB_AUTH_PATH):
        return False, f"USB 控制节点不存在: {USB_HUB_AUTH_PATH}", None

    try:
        with open(USB_HUB_AUTH_PATH, "r") as f:
            value = f.read().strip()

        return True, "", {
            "authorized": value,
            "enabled": value == "1",
            "path": USB_HUB_AUTH_PATH
        }

    except Exception as e:
        return False, f"读取 USB 状态失败: {e}", None


def write_usb_status(enable):
    """
    写 USB Hub 状态。
    enable = True  -> 写 1，启用
    enable = False -> 写 0，禁用
    """
    if not os.path.exists(USB_HUB_AUTH_PATH):
        return False, f"USB 控制节点不存在: {USB_HUB_AUTH_PATH}"

    value = "1" if enable else "0"

    try:
        with open(USB_HUB_AUTH_PATH, "w") as f:
            f.write(value)

        return True, ""

    except PermissionError:
        return False, "权限不足，请确认 Flask 后端是否以 root 权限运行"

    except Exception as e:
        return False, f"写入 USB 状态失败: {e}"

def now_str():
    return datetime.now().strftime('%Y-%m-%d %H:%M:%S')

def jdump(obj):
    try:
        return json.dumps(obj, ensure_ascii=False)
    except Exception as e:
        return f"<json-dump-error: {e}>"

def restart_service_and_exit(service_name: str, reason: str):
    """重启 systemd 服务并立刻退出当前进程"""
    print(f"[{now_str()}] [FATAL] 共享内存最终失败: {reason} → 重启服务: {service_name}")
    try:
        subprocess.run(
            ["systemctl", "restart", service_name],
            check=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
        )
    except Exception as e:
        print(f"[{now_str()}] [ERROR] systemctl restart 失败: {e}")
    os._exit(1)

def run_cmd(args, timeout=60):
    """运行命令并返回 (rc, stdout, stderr)"""
    p = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                       text=True, timeout=timeout)
    return p.returncode, p.stdout, p.stderr

def norm_dt(s: str) -> str:
    """
    规范化前端 datetime-local 字符串为 'YYYY-MM-DD HH:MM:SS'
    支持：
      - 'YYYY-MM-DDTHH:MM'        -> 秒补 ':00'
      - 'YYYY-MM-DDTHH:MM:SS'
      - 'YYYY-MM-DD HH:MM'        -> 秒补 ':00'
      - 'YYYY-MM-DD HH:MM:SS'
    其他输入返回空串（视作未提供）。
    """
    if not s or not isinstance(s, str):
        return ""
    s = s.strip()
    if not s:
        return ""
    s = s.replace('T', ' ')
    try:
        dt = datetime.strptime(s, "%Y-%m-%d %H:%M:%S")
        return dt.strftime("%Y-%m-%d %H:%M:%S")
    except ValueError:
        pass
    try:
        dt = datetime.strptime(s, "%Y-%m-%d %H:%M")
        return dt.strftime("%Y-%m-%d %H:%M:%S")
    except ValueError:
        return ""

def get_local_ip():
    env_ip = os.environ.get("APP_HOST", "").strip()
    if env_ip:
        return env_ip
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.connect(("8.8.8.8", 80))
            return s.getsockname()[0]
    except Exception:
        return "127.0.0.1"
def password_monitor():
    """
    后台检查密码过期状态。

    当前策略：
    - 不自动重置密码
    - 不修改 password
    - 不修改 password_created_at
    - 不修改 password_expire_at
    - 只打印过期用户日志
    - 真正要求用户改密码，由 /permission/login 返回 need_modify_password 给前端处理
    """
    while True:
        now = datetime.now()

        try:
            with engine.begin() as conn:
                rows = conn.execute(
                    text(f"""
                        SELECT username, password_expire_at
                        FROM `{PERM_TABLE}`
                        WHERE password_expire_at IS NOT NULL
                    """)
                ).fetchall()

                for username, expire_at in rows:
                    if expire_at and now > expire_at:
                        print(
                            f"[{now_str()}] [PASSWORD_EXPIRED] "
                            f"user={username}, expire_at={expire_at}, no reset"
                        )

        except Exception as e:
            print(f"[{now_str()}] [PASSWORD_MONITOR_ERROR] {e}")

        time.sleep(3600)
# ================== 基础初始化 ==================
app = Flask(__name__, static_folder="dist", static_url_path="/")
socketio = SocketIO(app)
HOST_IP = get_local_ip()
remote_client = ModbusTcpClient(HOST_IP, port=1502, keep_alive=True)
remote_client_lock = threading.Lock()
engine = create_engine(
    f'mysql+pymysql://root:qwer1234@{HOST_IP}:3306/log_db',
    pool_pre_ping=True,
    pool_recycle=3600
)

PERM_TABLE = "Permission_Management"
DEFAULT_PASSWORD = "12345678"
PASSWORD_LIFETIME_DAYS = 180
PASSWORD_WARNING_DAYS = 15
def ensure_permission_table() -> None:
    """
    确保 Permission_Management 表存在；不存在则创建并写入默认用户。
    表结构:
      username VARCHAR(64) 主键
      password VARCHAR(128)
      permission TINYINT (1-4)
      first_login TINYINT (0/1)
    """
    create_sql = f"""
    CREATE TABLE IF NOT EXISTS `{PERM_TABLE}` (
        `username` VARCHAR(64) NOT NULL,
        `password` VARCHAR(128) NOT NULL,
        `permission` TINYINT NOT NULL,
        `first_login` TINYINT NOT NULL,

        `password_created_at` DATETIME NULL,
        `password_expire_at` DATETIME NULL,
        `last_notify_at` DATETIME NULL,

        PRIMARY KEY (`username`)
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
    """
    default_user = {
        "username": "trinastorage",
        "password": "12345678",
        "permission": 4,
        "first_login": 0,
    }
    with engine.begin() as conn:
        conn.execute(text(create_sql))
        row = conn.execute(
            text(f"SELECT 1 FROM `{PERM_TABLE}` WHERE username=:username LIMIT 1"),
            {"username": default_user["username"]},
        ).fetchone()
        if row is None:
            conn.execute(
                text(
                    f"INSERT INTO `{PERM_TABLE}` (username, password, permission, first_login) "
                    f"VALUES (:username, :password, :permission, :first_login)"
                ),
                default_user,
            )

def init_permission_table():
    try:
        ensure_permission_table()
        print(f"[{now_str()}] [INIT] Permission table ready: {PERM_TABLE}")
    except Exception as e:
        print(f"[{now_str()}] [INIT] Permission table init failed: {e}")
# ===== 启动后台任务 =====
def start_background_tasks():
    threading.Thread(target=password_monitor, daemon=True).start()
init_permission_table()
start_background_tasks()
def ensure_remote_connected(context: str = "") -> bool:
    if remote_client.is_socket_open():
        return True
    ok = remote_client.connect()
    if not ok:
        print(f"[{now_str()}] [REMOTE] connect failed ({context})")
        return False
    print(f"[{now_str()}] [REMOTE] connected ({context})")
    return True

def write_register_with_retry(write_address: int, write_value: int, slave: int = 1):
    """
    ?????????????????????????????????????
    ??: (result, attempt_no)
    """
    with remote_client_lock:
        if not ensure_remote_connected("write_register"):
            raise RuntimeError("remote not connected")

        result = remote_client.write_register(write_address, write_value, slave=slave)
        if result is not None and (not result.isError()):
            return result, 1

        print(f"[{now_str()}] [REMOTE] ????????????: addr={write_address}, result={result}")
        remote_client.close()
        if not ensure_remote_connected("write_register_retry"):
            raise RuntimeError("remote reconnect failed")

        result2 = remote_client.write_register(write_address, write_value, slave=slave)
        return result2, 2
    


def get_special_0x10_range(addr: int):
    if 27046 <= addr <= 27052:
        return 27046, 7

    if 27346 <= addr <= 27352:
        return 27346, 7

    return None


def is_special_0x10_merge_enabled():
    pcs1_state = int(registers_model.get_input(17000 + 300 * 0 + 64))
    pcs2_state = int(registers_model.get_input(17000 + 300 * 1 + 64))

    return (((pcs1_state >> 0) & 1) == 1) or (((pcs2_state >> 0) & 1) == 1)


def write_registers_with_retry(write_address: int, values: list, slave: int = 1):
    with remote_client_lock:
        if not ensure_remote_connected("write_registers"):
            raise RuntimeError("remote not connected")

        result = remote_client.write_registers(write_address, values, slave=slave)
        if result is not None and (not result.isError()):
            return result, 1

        print(
            f"[{now_str()}] [REMOTE] 批量写失败，准备重连: "
            f"addr={write_address}, values={values}, result={result}"
        )

        remote_client.close()

        if not ensure_remote_connected("write_registers_retry"):
            raise RuntimeError("remote reconnect failed")

        result2 = remote_client.write_registers(write_address, values, slave=slave)
        return result2, 2
def get_special_0x10_range(addr: int):
    """
    判断地址是否属于需要转 0x10 批量写的范围。
    返回: (start_addr, quantity) 或 None
    """
    if 27046 <= addr <= 27052:
        return 27046, 7

    if 27346 <= addr <= 27352:
        return 27346, 7

    return None


def write_registers_with_retry(write_address: int, values: list, slave: int = 1):
    """
    0x10 功能码：批量写多个保持寄存器
    返回: (result, attempt_no)
    """
    with remote_client_lock:
        if not ensure_remote_connected("write_registers"):
            raise RuntimeError("remote not connected")

        result = remote_client.write_registers(write_address, values, slave=slave)
        if result is not None and (not result.isError()):
            return result, 1

        print(f"[{now_str()}] [REMOTE] 批量写失败，准备重连: addr={write_address}, values={values}, result={result}")
        remote_client.close()

        if not ensure_remote_connected("write_registers_retry"):
            raise RuntimeError("remote reconnect failed")

        result2 = remote_client.write_registers(write_address, values, slave=slave)
        return result2, 2
@app.after_request
def after_request(response):
    response.headers['X-Content-Type-Options'] = 'nosniff'
    response.headers['X-Frame-Options'] = 'DENY'
    response.headers['X-XSS-Protection'] = '1; mode=block'
    return response

# ================== 共享内存模型（严格失败即重启） ==================
SERVICE_NAME = "python_app.service"  # 失败时重启的服务

class RegistersModel:
    def __init__(self):
        path = '/dev/shm/my_shared_memory_logger'
        dtype = np.uint16
        self.Reg = None

        for attempt in (1, 2):
            try:
                if not os.path.exists(path):
                    raise RuntimeError(f"共享内存文件不存在: {path}")

                filesize = os.path.getsize(path)
                itemsize = np.dtype(dtype).itemsize
                if itemsize <= 0:
                    raise RuntimeError("dtype itemsize 非法")

                count = filesize // itemsize
                if count < 2 or (count % 2) != 0:
                    raise RuntimeError(f"元素数不合法（需要>=2且为偶数），实际: {count}")

                self.Reg = np.memmap(path, dtype=dtype, mode='r', shape=(count,))
                _ = int(self.Reg[0])  # 触发实际访问
                print(f"[{now_str()}] [INFO] RegistersModel 映射成功: {path}，元素个数: {count}")
                break
            except Exception as e:
                if attempt == 1:
                    print(f"[{now_str()}] [WARNING] RegistersModel 映射失败(第1次): {e}，5秒后重试 ...")
                    time.sleep(5)
                else:
                    restart_service_and_exit(SERVICE_NAME, f"RegistersModel 第2次失败: {e}")

        self.ALL_SIZE = len(self.Reg)
        self.INPUT_SIZE = self.ALL_SIZE // 2
        self.HOLD_SIZE = self.ALL_SIZE - self.INPUT_SIZE
        self.Input = self.Reg[:self.INPUT_SIZE]
        self.Hold = self.Reg[self.INPUT_SIZE:]

    def get_input(self, address):
        return self.Input[address] if 0 <= address < self.INPUT_SIZE else 0

    def get_hold(self, address):
        return self.Hold[address] if 0 <= address < self.HOLD_SIZE else 0

class RankModel:
    def __init__(self):
        self.BANK_SIZE = 10
        self.R_INPUT_SIZE = 17280
        self.path = '/dev/shm/my_shared_memory_bms'
        self.dtype = np.uint16
        expected_count = self.BANK_SIZE * self.R_INPUT_SIZE

        self.Reg = None
        self.Input = None

        for attempt in (1, 2):
            try:
                if not os.path.exists(self.path):
                    raise RuntimeError(f"共享内存文件不存在: {self.path}")

                filesize = os.path.getsize(self.path)
                itemsize = np.dtype(self.dtype).itemsize
                count = filesize // itemsize
                if count != expected_count:
                    raise RuntimeError(f"元素数不匹配，实际: {count}，预期: {expected_count}")

                self.Reg = np.memmap(self.path, dtype=self.dtype, mode='r', shape=(count,))
                self.Input = self.Reg.reshape(self.BANK_SIZE, self.R_INPUT_SIZE)
                _ = int(self.Input[0][0])
                print(f"[{now_str()}] [INFO] RankModel 映射成功: {self.path}, shape=({self.BANK_SIZE}, {self.R_INPUT_SIZE})")
                break
            except Exception as e:
                if attempt == 1:
                    print(f"[{now_str()}] [WARNING] RankModel 映射失败(第1次): {e}，15秒后重试 ...")
                    time.sleep(15)
                else:
                    restart_service_and_exit(SERVICE_NAME, f"RankModel 第2次失败: {e}")

    def get_r_input(self, bank_index, address):
        if 0 <= bank_index < self.BANK_SIZE and 0 <= address < self.R_INPUT_SIZE:
            return self.Input[bank_index][address]
        return 0

    def get_r_hold(self, bank_index, address):
        return self.get_r_input(bank_index, address)

# 初始化共享内存（若失败会在内部重启并退出）
registers_model = RegistersModel()
rank_model = RankModel()

def query_register_data(deviceid, functioncode, address, quantity):
    if functioncode == 3:
        return [registers_model.get_hold(address + j) for j in range(quantity)]
    else:
        return [registers_model.get_input(address + j) for j in range(quantity)]

def query_rank_register_data(bank_index, functioncode, address, quantity):
    if functioncode == 3:
        return [rank_model.get_r_hold(bank_index, address + j) for j in range(quantity)]
    else:
        return [rank_model.get_r_input(bank_index, address + j) for j in range(quantity)]

# ================== 同步 pcs_ware 配置（显式私钥/端口/用户） ==================
REMOTE_USER = "zlg"
REMOTE_HOST = "192.168.1.163"
REMOTE_PORT = 22
REMOTE_DIR  = "/home/zlg/pcs_ware"
LOCAL_DIR   = Path("/home/zlg/fault_wave")
ZLOG_DIR = Path("/home/zlgmcu/zlog")

SSH_KEY_PATH = "/root/.ssh/id_ed25519"   # 如不需要可设为 "" 或 None

def _base_ssh_args():
    args = ["ssh", "-o", "StrictHostKeyChecking=no", "-p", str(REMOTE_PORT)]
    if SSH_KEY_PATH:
        args += ["-i", SSH_KEY_PATH]
    return args

def _base_scp_args():
    args = ["scp", "-q", "-o", "StrictHostKeyChecking=no", "-P", str(REMOTE_PORT)]
    if SSH_KEY_PATH:
        args += ["-i", SSH_KEY_PATH]
    return args

# ================== 拉取接口 & 诊断接口 ==================
@app.route('/fault_wave/fetch', methods=['POST'])
def fetch_wave():
    """
    固定拉取 pcs1.db 文件（覆盖本地），并返回本地已有的全部文件名(local_files)。
    """
    LOCAL_DIR.mkdir(parents=True, exist_ok=True)

    def list_local_files():
        return sorted([p.name for p in LOCAL_DIR.iterdir() if p.is_file()])

    ssh_target = f"{REMOTE_USER}@{REMOTE_HOST}"

    # 只拉取固定的文件名 pcs1.db
    list_cmd = _base_ssh_args() + [
        ssh_target,
        f"find {REMOTE_DIR} -maxdepth 1 -type f -name 'pcs1.db' -printf '%f\\n'"
    ]
    rc, out, err = run_cmd(list_cmd, timeout=30)

    if rc != 0:
        all_local = list_local_files()
        return jsonify({
            "ok": False,
            "error": f"列远端目录失败: {err.strip()}",
            "dest_dir": str(LOCAL_DIR),
            "local_files": all_local,
            "local_count": len(all_local),
            "fetched": [],
            "errors": []
        }), 500

    remote_names = [line.strip() for line in out.splitlines() if line.strip() == "pcs1.db"]
    if not remote_names:
        all_local = list_local_files()
        return jsonify({
            "ok": False,
            "error": "远端未找到 pcs1.db 文件",
            "dest_dir": str(LOCAL_DIR),
            "local_files": all_local,
            "local_count": len(all_local),
            "fetched": [],
            "errors": []
        }), 404

    # 无条件拉取并覆盖
    fetched, errors = [], []
    for name in remote_names:
        remote_path = f"{ssh_target}:{REMOTE_DIR}/{name}"
        local_path  = str(LOCAL_DIR / name)
        scp_cmd = _base_scp_args() + [remote_path, local_path]
        rc, _o, _e = run_cmd(scp_cmd, timeout=120)
        if rc == 0:
            fetched.append(name)
        else:
            errors.append({"file": name, "error": _e.strip()})

    # 返回结果
    all_local = list_local_files()
    ok = (len(errors) == 0)
    return jsonify({
        "ok": ok,
        "message": f"已拉取 pcs1.db（覆盖）" if ok else f"拉取失败",
        "dest_dir": str(LOCAL_DIR),
        "fetched": fetched,
        "errors": errors,
        "local_files": all_local,
        "local_count": len(all_local)
    }), (200 if ok else 207)

@app.route('/fault_wave/diag', methods=['GET'])
def fault_wave_diag():
    """快速诊断 SSH/目录/权限问题"""
    ssh_target = f"{REMOTE_USER}@{REMOTE_HOST}"
    checks = {}

    def add_check(name, cmd, timeout=10):
        rc, so, se = run_cmd(cmd, timeout=timeout)
        checks[name] = {"rc": rc, "stdout": so, "stderr": se}

    add_check("whoami_local", ["bash", "-lc", "whoami; id; ls -ld ~/.ssh ~/.ssh/id_* ~/.ssh/authorized_keys || true"])
    add_check("ssh_version", _base_ssh_args() + ["-V"])
    add_check("ssh_echo", _base_ssh_args() + [ssh_target, "echo OK && whoami && ls -ld ~/.ssh ~/.ssh/authorized_keys || true"])
    add_check("remote_ls", _base_ssh_args() + [ssh_target, f"ls -l {REMOTE_DIR} || true"])
    add_check("remote_find", _base_ssh_args() + [ssh_target, f"find {REMOTE_DIR} -maxdepth 1 -type f -printf '%f\\n' || true"])

    return jsonify({
        "config": {
            "REMOTE_USER": REMOTE_USER,
            "REMOTE_HOST": REMOTE_HOST,
            "REMOTE_PORT": REMOTE_PORT,
            "REMOTE_DIR": REMOTE_DIR,
            "LOCAL_DIR": str(LOCAL_DIR),
            "SSH_KEY_PATH": SSH_KEY_PATH,
        },
        "checks": checks
    })
@app.route("/system/reboot", methods=["POST"])
def system_reboot():
    data = request.get_json(silent=True) or {}

    if data.get("cmd") != "reboot":
        return jsonify({
            "result": "error",
            "error": "invalid cmd"
        }), 400

    reboot_err = schedule_reboot(2)
    if reboot_err is not None:
        return jsonify({
            "result": "error",
            "error": reboot_err
        }), 500

    return jsonify({
        "result": "ok"
    }), 200
# === 统计指定本地 sqlite 的行数（支持时间范围） ===
@app.route('/sqlite/count_rows', methods=['POST'])
def sqlite_count_rows():
    """
    Request(JSON):
      { "file": "xxx.db", "table": "fault_logs",
        "start_time": "YYYY-MM-DD HH:MM[:SS]|YYYY-MM-DDTHH:MM[:SS]",
        "end_time":   "..." }
    """
    try:
        data = request.get_json(force=True) or {}
        fname = str(data.get("file", "")).strip()
        table = str(data.get("table", "")).strip()
        start_time = norm_dt(str(data.get("start_time", "")).strip())
        end_time   = norm_dt(str(data.get("end_time", "")).strip())

        if not fname:
            return jsonify({"ok": False, "error": "缺少 file"}), 400
        if not table:
            return jsonify({"ok": False, "error": "缺少 table"}), 400

        safe_name = os.path.basename(fname)
        db_path = LOCAL_DIR / safe_name
        if not db_path.is_file():
            return jsonify({"ok": False, "error": f"数据库不存在: {db_path}"}), 404

        if not table.replace('_', '').isalnum():
            return jsonify({"ok": False, "error": "非法表名（仅允许字母数字下划线）"}), 400

        uri = f"file:{db_path}?mode=ro"
        conn = sqlite3.connect(uri, uri=True)
        try:
            cur = conn.cursor()
            cur.execute("SELECT name FROM sqlite_master WHERE type='table' AND name=?;", (table,))
            if cur.fetchone() is None:
                return jsonify({"ok": False, "error": f"表不存在: {table}"}), 404

            where = ""
            params = []
            if start_time and end_time:
                where = 'WHERE "fault_time" BETWEEN ? AND ?'
                params = [start_time, end_time]
            elif start_time:
                where = 'WHERE "fault_time" >= ?'
                params = [start_time]
            elif end_time:
                where = 'WHERE "fault_time" <= ?'
                params = [end_time]

            sql = f'SELECT COUNT(*) FROM "{table}" {where}'
            cur.execute(sql, params)
            row = cur.fetchone()
            count = int(row[0]) if row else 0
        finally:
            conn.close()

        return jsonify({"ok": True, "file": safe_name, "table": table, "count": count})
    except Exception as e:
        return jsonify({"ok": False, "error": str(e)}), 500

# === 分帧查询列数据（支持时间范围） ===
@app.route('/sqlite/query_series', methods=['POST'])
def sqlite_query_series():
    """
    Request(JSON):
      {
        "file": "test.db",
        "table": "fault_logs",
        "columns": ["v_rs", "v_ts"],
        "frame": 1,
        "frame_size": 500,
        "start_time": "...",
        "end_time":   "..."
      }
    """
    try:
        data = request.get_json(force=True) or {}
        fname   = str(data.get("file", "")).strip()
        table   = str(data.get("table", "")).strip()
        columns = data.get("columns", [])
        frame   = int(data.get("frame", 1))
        fsize   = int(data.get("frame_size", 500))
        start_time = norm_dt(str(data.get("start_time", "")).strip())
        end_time   = norm_dt(str(data.get("end_time", "")).strip())

        if not fname or not table or not columns:
            return jsonify({"ok": False, "error": "缺少 file/table/columns"}), 400
        if frame < 1 or fsize < 1:
            return jsonify({"ok": False, "error": "frame/frame_size 非法"}), 400

        if not table.replace('_', '').isalnum():
            return jsonify({"ok": False, "error": "非法表名"}), 400
        for c in columns:
            if not isinstance(c, str) or not c.replace('_', '').isalnum():
                return jsonify({"ok": False, "error": f"非法列名: {c}"}), 400

        safe_name = os.path.basename(fname)
        db_path = LOCAL_DIR / safe_name
        if not db_path.is_file():
            return jsonify({"ok": False, "error": f"数据库不存在: {db_path}"}), 404

        offset = (frame - 1) * fsize
        sel_cols = ", ".join([f'"{c}"' for c in columns])

        where = ""
        params = []
        if start_time and end_time:
            where = 'WHERE "fault_time" BETWEEN ? AND ?'
            params.extend([start_time, end_time])
        elif start_time:
            where = 'WHERE "fault_time" >= ?'
            params.append(start_time)
        elif end_time:
            where = 'WHERE "fault_time" <= ?'
            params.append(end_time)

        sql = f'SELECT {sel_cols} FROM "{table}" {where} ORDER BY "id" LIMIT ? OFFSET ?'
        params.extend([fsize, offset])

        uri = f"file:{db_path}?mode=ro"
        conn = sqlite3.connect(uri, uri=True)
        try:
            cur = conn.cursor()
            cur.execute(sql, params)
            rows = cur.fetchall()
        finally:
            conn.close()

        series = {c: [] for c in columns}
        for r in rows:
            for idx, c in enumerate(columns):
                series[c].append(r[idx])

        return jsonify({
            "ok": True,
            "file": safe_name,
            "table": table,
            "start": offset,
            "limit": fsize,
            "rows": len(rows),
            "series": series
        })
    except Exception as e:
        return jsonify({"ok": False, "error": str(e)}), 500

# === zlog csv: 按时间范围读取第二列 ===
@app.route('/zlog/query_series', methods=['POST'])
def zlog_query_series():
    """
    Request(JSON):
      {
        "file": "operation20240101" | "operation20240101.csv",
        "start_time": "YYYY-MM-DD HH:MM[:SS]|YYYY-MM-DDTHH:MM[:SS]",
        "end_time":   "..."
      }
    CSV 格式:
      第一列: 时间(精确到秒)
      第二列: 数值
    """
    try:
        data = request.get_json(force=True) or {}
        fname = str(data.get("file", "")).strip()
        start_time = norm_dt(str(data.get("start_time", "")).strip())
        end_time   = norm_dt(str(data.get("end_time", "")).strip())

        if not fname:
            return jsonify({"ok": False, "error": "缺少 file"}), 400

        safe_name = os.path.basename(fname)
        if not safe_name.lower().endswith(".csv"):
            safe_name = safe_name + ".csv"

        if not re.fullmatch(r"operation_?\d{8}\.csv", safe_name):
            return jsonify({"ok": False, "error": "file 必须是 operationYYYYMMDD 或 operation_YYYYMMDD（可带 .csv）"}), 400

        csv_path = ZLOG_DIR / safe_name
        if not csv_path.is_file():
            alt_name = None
            if re.fullmatch(r"operation\d{8}\.csv", safe_name):
                alt_name = safe_name.replace("operation", "operation_", 1)
            elif re.fullmatch(r"operation_\d{8}\.csv", safe_name):
                alt_name = safe_name.replace("operation_", "operation", 1)
            if alt_name:
                alt_path = ZLOG_DIR / alt_name
                if alt_path.is_file():
                    csv_path = alt_path
                    safe_name = alt_name
                else:
                    return jsonify({"ok": False, "error": f"文件不存在: {csv_path}"}), 404
            else:
                return jsonify({"ok": False, "error": f"文件不存在: {csv_path}"}), 404

        if not start_time and not end_time:
            return jsonify({"ok": False, "error": "start_time/end_time 至少提供一个"}), 400

        start_dt = datetime.strptime(start_time, "%Y-%m-%d %H:%M:%S") if start_time else None
        end_dt = datetime.strptime(end_time, "%Y-%m-%d %H:%M:%S") if end_time else None

        results = []
        with open(csv_path, "rb") as f:
            raw = f.read().replace(b"\x00", b"")
        text = raw.decode("utf-8", errors="ignore")
        reader = csv.reader(io.StringIO(text))
        for row in reader:
            if len(row) < 2:
                continue
            ts_raw = str(row[0]).strip()
            val_raw = str(row[1]).strip()
            if not ts_raw:
                continue
            try:
                ts = datetime.strptime(ts_raw, "%Y-%m-%d %H:%M:%S")
            except ValueError:
                continue

            if start_dt and ts < start_dt:
                continue
            if end_dt and ts > end_dt:
                continue

            results.append({
                "time": ts_raw,
                "value": val_raw
            })

        return jsonify({
            "ok": True,
            "file": safe_name,
            "start_time": start_time or None,
            "end_time": end_time or None,
            "count": len(results),
            "values": results
        })
    except Exception as e:
        return jsonify({"ok": False, "error": str(e)}), 500

# ================== 业务路由 ==================
@app.route('/export_csv_stream', methods=['GET'])
def export_csv_stream():
    table_name = request.args.get('table')
    if not table_name or not table_name.replace('_', '').isalnum():
        print(f"[{now_str()}] [CSV导出] 非法表名: {table_name}")
        return jsonify({'error': 'Invalid table name'}), 400

    print(f"[{now_str()}] [CSV导出] 开始导出表: {table_name}")

    def generate():
        conn = None
        cursor = None
        row_count = 0
        try:
            conn = pymysql.connect(
                host=HOST_IP, user='root', password='qwer1234',
                db='log_db', charset='utf8mb4', cursorclass=pymysql.cursors.SSCursor
            )
            cursor = conn.cursor()
            cursor.execute(f"SELECT * FROM `{table_name}`")
            headers = [desc[0] for desc in cursor.description]
            print(f"[{now_str()}] [CSV导出] 表头: {headers}")

            output = io.StringIO()
            writer = csv.writer(output)
            writer.writerow(headers)
            yield output.getvalue()
            output.seek(0); output.truncate(0)

            for row in cursor:
                row_count += 1
                writer.writerow(row)
                if row_count % 1000 == 0:
                    print(f"[{now_str()}] [CSV导出] 已输出 {row_count} 行...")
                yield output.getvalue()
                output.seek(0); output.truncate(0)
        except Exception as e:
            print(f"[{now_str()}] [CSV导出] 异常: {e}")
            output = io.StringIO()
            writer = csv.writer(output)
            writer.writerow(["error", str(e)])
            yield output.getvalue()
        finally:
            if cursor is not None:
                cursor.close()
            if conn is not None:
                conn.close()
            print(f"[{now_str()}] [CSV导出] 完成，总行数（不含表头）: {row_count}")

    response = Response(generate(), mimetype='text/csv')
    response.headers.set('Content-Disposition', 'attachment', filename=f'{table_name}.csv')
    return response

@app.route('/api', methods=['GET', 'POST', 'OPTIONS'])
def GetData():
    if request.method == 'GET':
        sql = request.args.get("sql")
        params = request.args.get("params")
        if not sql or params != 'root':
            err = {'error': 'invalid params'}
            print(f"[{now_str()}] [GET /api] 参数非法: sql={sql}, params={params}")
            return jsonify(err), 400
        try:
            with engine.connect() as conn:
                result = conn.execute(text(sql))
                rows = result.fetchall()
                data_packet_arr = [','.join(map(str, row)) for row in rows]
                protocol = ':\r'.join(data_packet_arr)
                response_data = {"sql": sql, "params": params, "result": protocol}
                print(f"[{now_str()}] [GET响应] 返回数据: {jdump(response_data)}")

                response = make_response(json.dumps(response_data))
                response.headers['Access-Control-Allow-Origin'] = '*'
                return response
        except SQLAlchemyError as e:
            print(f"[{now_str()}] [GET /api] SQLAlchemy 异常: {e}")
            return jsonify({"error": str(e)}), 500

    elif request.method == 'POST':
        bodyData = request.get_json()
        print(f"[{now_str()}] [POST /api] 接收到前端数据: {jdump(bodyData)}")

        required_keys = ['deviceid', 'functioncode', 'Address', 'Quantity']
        if not bodyData or not all(k in bodyData for k in required_keys):
            err = {'error': 'Missing required keys.'}
            print(f"[{now_str()}] [POST /api] 缺少必要字段")
            return jsonify(err), 400

        try:
            deviceid = list(map(int, bodyData['deviceid'].split(',')))
            functioncode = list(map(int, bodyData['functioncode'].split(',')))
            address = list(map(int, bodyData['Address'].split(',')))
            quantity = list(map(int, bodyData['Quantity'].split(',')))
            value_list = list(map(int, bodyData.get('value', '0').split(',')))
        except ValueError:
            err = {'error': 'Invalid value types.'}
            print(f"[{now_str()}] [POST /api] 类型转换错误")
            return jsonify(err), 400

        if any(q < 1 for q in quantity):
            err = {'error': 'Quantity is out of range.'}
            print(f"[{now_str()}] [POST /api] Quantity 越界: {quantity}")
            return jsonify(err), 400

        data_packet_arr = []
        skip_indexes = set()

        merge_enabled = is_special_0x10_merge_enabled()

        for i in range(len(deviceid)):
            if i in skip_indexes:
                continue

            if functioncode[i] == 6:
                write_address = address[i]
                value = value_list[i]
                write_value = (value + 0x10000) if value < 0 else value

                if merge_enabled:
                    special_range = get_special_0x10_range(write_address)
                else:
                    special_range = None

                if special_range is not None:
                    start_addr, reg_count = special_range

                    values = [
                        int(registers_model.get_hold(start_addr + j))
                        for j in range(reg_count)
                    ]

                    merged_addrs = []

                    for k in range(len(deviceid)):
                        if functioncode[k] != 6:
                            continue

                        if k in skip_indexes:
                            continue

                        addr_k = address[k]
                        range_k = get_special_0x10_range(addr_k)

                        if range_k is None:
                            continue

                        start_k, count_k = range_k

                        if start_k == start_addr and count_k == reg_count:
                            val_k = value_list[k]
                            write_val_k = (val_k + 0x10000) if val_k < 0 else val_k

                            offset_k = addr_k - start_addr
                            values[offset_k] = write_val_k

                            merged_addrs.append(addr_k)
                            skip_indexes.add(k)

                    try:
                        result, attempt_no = write_registers_with_retry(
                            start_addr,
                            values,
                            slave=1
                        )

                        if result is not None and (not result.isError()):
                            strval = (
                                f"ok_0x10({start_addr}-{start_addr + reg_count - 1}, "
                                f"merged={merged_addrs})"
                            )
                        else:
                            print(
                                f"[{now_str()}] [POST /api] 0x10批量写失败: "
                                f"start_addr={start_addr}, "
                                f"reg_count={reg_count}, "
                                f"values={values}, "
                                f"merged_addrs={merged_addrs}, "
                                f"attempt={attempt_no}, "
                                f"result={result}"
                            )
                            strval = f"error_0x10({start_addr})"

                    except Exception as e:
                        print(
                            f"[{now_str()}] [POST /api] 0x10批量写异常: "
                            f"start_addr={start_addr}, "
                            f"reg_count={reg_count}, "
                            f"values={values}, "
                            f"merged_addrs={merged_addrs}, "
                            f"err={e}"
                        )

                        with remote_client_lock:
                            remote_client.close()

                        strval = f"exception_0x10({start_addr})"

                    data_packet_arr.append(strval)
                    continue

                try:
                    result, attempt_no = write_register_with_retry(
                        write_address,
                        write_value,
                        slave=1
                    )

                    if result is not None and (not result.isError()):
                        strval = f"ok({write_address})"
                    else:
                        print(
                            f"[{now_str()}] [POST /api] 写寄存器失败: "
                            f"addr={write_address}, "
                            f"attempt={attempt_no}, "
                            f"result={result}"
                        )
                        strval = f"error({write_address})"

                except Exception as e:
                    print(
                        f"[{now_str()}] [POST /api] 写寄存器异常: "
                        f"addr={write_address}, err={e}"
                    )

                    with remote_client_lock:
                        remote_client.close()

                    strval = f"exception({write_address})"

                data_packet_arr.append(strval)

            elif deviceid[i] == 1:
                result = query_register_data(
                    deviceid[i],
                    functioncode[i],
                    address[i],
                    quantity[i]
                )
                data_packet_arr.append(','.join(map(str, result)))

            else:
                result = query_rank_register_data(
                    deviceid[i] - 2,
                    functioncode[i],
                    address[i],
                    quantity[i]
                )
                data_packet_arr.append(','.join(map(str, result)))

        ret = [
            {'deviceid': bodyData['deviceid']},
            {'functioncode': bodyData['functioncode']},
            {'Address': bodyData['Address']},
            {'Quantity': bodyData['Quantity']},
            {'Registers': ':'.join(data_packet_arr)}
        ]

        print(f"[{now_str()}] [POST响应] 返回数据: {jdump(ret)}")

        response = make_response(json.dumps(ret))
        response.headers['Access-Control-Allow-Origin'] = '*'
        return response

# ================== 权限管理 API ==================
def _perm_table_exists(conn) -> bool:
    row = conn.execute(
        text("SHOW TABLES LIKE :tname"),
        {"tname": PERM_TABLE},
    ).fetchone()
    return row is not None

def _ensure_perm_table_in_request():
    try:
        ensure_permission_table()
        return None
    except Exception as e:
        return str(e)

@app.route("/permission/login", methods=["POST"])
def permission_login():
    """
    登录校验：
    请求：
    {
        "username": "用户名",
        "password": "密码"
    }

    返回：
    - ok
    - db_status
    - username
    - permission
    - first_login
    - password_warning      是否进入密码到期提醒期
    - password_expired      密码是否已经过期
    - password_expire_at    密码过期时间
    - password_days_left    密码剩余天数
    - message               提示信息

    说明：
    - 密码过期后不重置密码
    - 密码过期后不修改数据库中的密码字段
    - 密码过期后仍允许登录
    - 前端根据 password_expired 判断是否强制跳转修改密码页面
    """

    # =========================
    # 1. 初始化权限表
    # =========================
    err = _ensure_perm_table_in_request()
    if err:
        return jsonify({
            "ok": False,
            "db_status": 0,
            "error": f"Permission table initialization failed: {err}"
        }), 500

    # =========================
    # 2. 获取请求参数
    # =========================
    data = request.get_json(force=True) or {}
    username = str(data.get("username", "")).strip()
    password = str(data.get("password", "")).strip()

    if not username or not password:
        return jsonify({
            "ok": False,
            "db_status": 1,
            "error": "Missing username/password"
        }), 400

    now = datetime.now()

    try:
        with engine.begin() as conn:

            # =========================
            # 3. 检查权限表是否存在
            # =========================
            if not _perm_table_exists(conn):
                return jsonify({
                    "ok": False,
                    "db_status": 0,
                    "error": "Permission table does not exist"
                }), 500

            # =========================
            # 4. 查询用户
            # 注意：
            # 密码过期不等于密码错误。
            # 只要用户名和密码匹配，就允许登录。
            # =========================
            row = conn.execute(
                text(f"""
                    SELECT
                        permission,
                        first_login,
                        password_created_at,
                        password_expire_at
                    FROM `{PERM_TABLE}`
                    WHERE username=:username AND password=:password
                    LIMIT 1
                """),
                {
                    "username": username,
                    "password": password,
                },
            ).fetchone()

            # =========================
            # 5. 账号或密码错误
            # =========================
            if row is None:
                return jsonify({
                    "ok": False,
                    "db_status": 1,
                    "error": "Invalid username or password"
                }), 401

            permission, first_login, created_at, expire_at = row

            password_warning = False
            password_expired = False
            password_days_left = None
            message = ""

            # =========================
            # 6. 判断密码生命周期
            # =========================
            if expire_at is not None:
                delta = expire_at - now
                total_seconds_left = delta.total_seconds()

                if total_seconds_left < 0:
                    # =========================
                    # 密码已经过期
                    # =========================
                    # 重要：
                    # 这里不重置密码
                    # 这里不修改 password
                    # 这里不修改 password_created_at
                    # 这里不修改 password_expire_at
                    # 这里不修改 last_notify_at
                    # 仍然允许登录
                    password_expired = True
                    password_warning = False
                    password_days_left = 0
                    message = "password expired, please modify password"

                elif delta <= timedelta(days=PASSWORD_WARNING_DAYS):
                    # =========================
                    # 密码即将过期
                    # =========================
                    password_expired = False
                    password_warning = True
                    password_days_left = delta.days
                    message = "password will expire soon, please modify password"

                    # 只记录提醒时间，不修改密码本身
                    conn.execute(
                        text(f"""
                            UPDATE `{PERM_TABLE}`
                            SET last_notify_at=:now
                            WHERE username=:username
                        """),
                        {
                            "now": now,
                            "username": username,
                        },
                    )

                else:
                    # =========================
                    # 密码正常
                    # =========================
                    password_expired = False
                    password_warning = False
                    password_days_left = delta.days
                    message = ""

            # =========================
            # 7. 登录成功返回
            # =========================
            return jsonify({
                "ok": True,
                "db_status": 1,
                "username": username,
                "permission": int(permission),
                "first_login": int(first_login),

                "password_warning": password_warning,
                "password_expired": password_expired,
                "password_expire_at": expire_at.strftime("%Y-%m-%d %H:%M:%S") if expire_at else None,
                "password_days_left": password_days_left,
                "message": message,
            })

    except Exception as e:
        return jsonify({
            "ok": False,
            "db_status": 0,
            "error": str(e)
        }), 500
@app.route("/permission/delete", methods=["POST"])
def permission_delete():
    """
    删除用户：请求 {username}
    """
    err = _ensure_perm_table_in_request()
    if err:
        return jsonify({"ok": False, "error": f"Permission table initialization failed: {err}"}), 500

    data = request.get_json(force=True) or {}
    username = str(data.get("username", "")).strip()
    if not username:
        return jsonify({"ok": False, "error": "Missing username"}), 400

    try:
        with engine.begin() as conn:
            if not _perm_table_exists(conn):
                return jsonify({"ok": False, "error": "Permission table does not exist"}), 500
            row = conn.execute(
                text(
                    f"SELECT 1 FROM `{PERM_TABLE}` "
                    f"WHERE username=:username LIMIT 1"
                ),
                {"username": username},
            ).fetchone()
            if row is None:
                return jsonify({"ok": False, "error": "Invalid username"}), 401
            conn.execute(
                text(f"DELETE FROM `{PERM_TABLE}` WHERE username=:username"),
                {"username": username},
            )
        return jsonify({"ok": True, "deleted": username})
    except Exception as e:
        return jsonify({"ok": False, "error": str(e)}), 500

@app.route("/permission/add", methods=["POST"])
def permission_add():
    """
    增加用户：
    请求 {username, permission}

    默认：
    - password = 12345678
    - first_login = 1
    - 密码有效期 = 6个月
    """

    err = _ensure_perm_table_in_request()
    if err:
        return jsonify({
            "ok": False,
            "error": f"Permission table initialization failed: {err}"
        }), 500

    data = request.get_json(force=True) or {}
    username = str(data.get("username", "")).strip()
    permission = data.get("permission")

    if not username or permission is None:
        return jsonify({
            "ok": False,
            "error": "Missing username/permission"
        }), 400

    # ===== 权限校验 =====
    try:
        permission = int(permission)
    except Exception:
        return jsonify({
            "ok": False,
            "error": "permission must be an integer"
        }), 400

    if permission < 1 or permission > 4:
        return jsonify({
            "ok": False,
            "error": "permission must be between 1 and 4"
        }), 400

    # ===== 默认密码策略 =====
    password = "12345678"
    first_login = 1

    now = datetime.now()
    expire_time = now + timedelta(days=180)  # 6个月

    try:
        with engine.begin() as conn:

            # ===== 检查表是否存在 =====
            if not _perm_table_exists(conn):
                return jsonify({
                    "ok": False,
                    "error": "Permission table does not exist"
                }), 500

            # ===== 用户是否已存在 =====
            exists = conn.execute(
                text(f"""
                    SELECT 1 FROM `{PERM_TABLE}`
                    WHERE username=:username
                    LIMIT 1
                """),
                {"username": username},
            ).fetchone()

            if exists is not None:
                return jsonify({
                    "ok": False,
                    "error": "User already exists"
                }), 409

            # ===== 插入用户（含密码生命周期）=====
            conn.execute(
                text(f"""
                    INSERT INTO `{PERM_TABLE}`
                    (
                        username,
                        password,
                        permission,
                        first_login,
                        password_created_at,
                        password_expire_at,
                        last_notify_at
                    )
                    VALUES
                    (
                        :username,
                        :password,
                        :permission,
                        :first_login,
                        :created_at,
                        :expire_at,
                        NULL
                    )
                """),
                {
                    "username": username,
                    "password": password,
                    "permission": permission,
                    "first_login": first_login,
                    "created_at": now,
                    "expire_at": expire_time,
                },
            )

        return jsonify({
            "ok": True,
            "username": username,
            "password": password,
            "permission": permission,
            "first_login": first_login,
            "password_created_at": now.strftime("%Y-%m-%d %H:%M:%S"),
            "password_expire_at": expire_time.strftime("%Y-%m-%d %H:%M:%S")
        })

    except Exception as e:
        return jsonify({
            "ok": False,
            "error": str(e)
        }), 500

@app.route("/permission/update", methods=["POST"])
def permission_update():
    """
    修改用户：
    请求参数：
    {
        "username": "当前用户名",

        可选：
        "new_username": "新用户名",
        "password": "新密码",
        "permission": 1~4,
        "first_login": 0或1
    }

    说明：
    - username 是当前用户名，用于定位数据库里的用户
    - new_username 可选，用于修改用户名
    - password 可选，修改密码时会刷新密码生命周期
    - 如果该用户 password_expire_at 为 NULL，表示永久账号，修改密码后仍保持永久
    """

    err = _ensure_perm_table_in_request()
    if err:
        return jsonify({
            "ok": False,
            "error": f"Permission table initialization failed: {err}"
        }), 500

    data = request.get_json(force=True) or {}

    username = str(data.get("username", "")).strip()
    new_username = str(data.get("new_username", "")).strip() if data.get("new_username") is not None else ""

    new_password = data.get("password")
    new_permission = data.get("permission")
    new_first_login = data.get("first_login")

    if not username:
        return jsonify({
            "ok": False,
            "error": "Missing username"
        }), 400

    updates = {}
    now = datetime.now()

    # =========================
    # 1. 处理新用户名
    # =========================
    if new_username:
        updates["username"] = new_username

    # =========================
    # 2. 处理新密码
    # =========================
    if new_password is not None:
        new_password = str(new_password).strip()

        if not new_password:
            return jsonify({
                "ok": False,
                "error": "password cannot be empty"
            }), 400

        updates["password"] = new_password
        updates["password_created_at"] = now
        updates["last_notify_at"] = None

    # =========================
    # 3. 处理权限
    # =========================
    if new_permission is not None:
        try:
            new_permission = int(new_permission)
        except Exception:
            return jsonify({
                "ok": False,
                "error": "permission must be an integer"
            }), 400

        if new_permission < 1 or new_permission > 4:
            return jsonify({
                "ok": False,
                "error": "permission must be between 1 and 4"
            }), 400

        updates["permission"] = new_permission

    # =========================
    # 4. 处理 first_login
    # =========================
    if new_first_login is not None:
        try:
            new_first_login = int(new_first_login)
        except Exception:
            return jsonify({
                "ok": False,
                "error": "first_login must be an integer"
            }), 400

        if new_first_login not in (0, 1):
            return jsonify({
                "ok": False,
                "error": "first_login must be 0 or 1"
            }), 400

        updates["first_login"] = new_first_login

    if not updates:
        return jsonify({
            "ok": False,
            "error": "No fields to update"
        }), 400

    try:
        with engine.begin() as conn:

            # =========================
            # 5. 检查权限表是否存在
            # =========================
            if not _perm_table_exists(conn):
                return jsonify({
                    "ok": False,
                    "error": "Permission table does not exist"
                }), 500

            # =========================
            # 6. 修改前先判断用户是否存在
            #    同时读取 password_expire_at 判断是否永久账号
            # =========================
            row = conn.execute(
                text(f"""
                    SELECT password_expire_at
                    FROM `{PERM_TABLE}`
                    WHERE username=:username
                    LIMIT 1
                """),
                {
                    "username": username
                }
            ).fetchone()

            if row is None:
                return jsonify({
                    "ok": False,
                    "error": "User not found"
                }), 404

            old_password_expire_at = row[0]

            # =========================
            # 7. 如果修改用户名，检查新用户名是否已存在
            # =========================
            if new_username and new_username != username:
                exists = conn.execute(
                    text(f"""
                        SELECT 1
                        FROM `{PERM_TABLE}`
                        WHERE username=:new_username
                        LIMIT 1
                    """),
                    {
                        "new_username": new_username
                    }
                ).fetchone()

                if exists is not None:
                    return jsonify({
                        "ok": False,
                        "error": "New username already exists"
                    }), 409

            # =========================
            # 8. 如果修改密码，刷新密码生命周期
            # =========================
            if new_password is not None:
                if old_password_expire_at is None:
                    # password_expire_at 为 NULL 表示永久账号
                    # 永久账号修改密码后仍保持永久
                    updates["password_expire_at"] = None
                else:
                    updates["password_expire_at"] = now + timedelta(days=PASSWORD_LIFETIME_DAYS)

            # =========================
            # 9. 动态拼接 UPDATE
            # =========================
            set_clauses = []
            params = {
                "old_username": username
            }

            for key, value in updates.items():
                set_clauses.append(f"`{key}`=:{key}")
                params[key] = value

            sql = f"""
                UPDATE `{PERM_TABLE}`
                SET {', '.join(set_clauses)}
                WHERE username=:old_username
            """

            conn.execute(text(sql), params)

        return jsonify({
            "ok": True,
            "username": updates.get("username", username),
            "updated_fields": list(updates.keys())
        })

    except Exception as e:
        return jsonify({
            "ok": False,
            "error": str(e)
        }), 500
@app.route("/permission/all", methods=["POST"])
def permission_all():
    """
    查询全部用户：返回 Permission_Management 全部内容
    包含密码生命周期字段，方便前端显示密码到期时间/是否即将过期。
    """
    err = _ensure_perm_table_in_request()
    if err:
        return jsonify({
            "ok": False,
            "error": f"Permission table initialization failed: {err}"
        }), 500

    try:
        now = datetime.now()

        with engine.begin() as conn:
            if not _perm_table_exists(conn):
                return jsonify({
                    "ok": False,
                    "error": "Permission table does not exist"
                }), 500

            rows = conn.execute(
                text(f"""
                    SELECT
                        username,
                        password,
                        permission,
                        first_login,
                        password_created_at,
                        password_expire_at,
                        last_notify_at
                    FROM `{PERM_TABLE}`
                    ORDER BY username
                """)
            ).fetchall()

        data = []
        for r in rows:
            username = r[0]
            password = r[1]
            permission = int(r[2])
            first_login = int(r[3])
            password_created_at = r[4]
            password_expire_at = r[5]
            last_notify_at = r[6]

            password_warning = False
            password_expired = False
            password_days_left = None

            if password_expire_at is not None:
                delta = password_expire_at - now
                password_days_left = delta.days

                if delta.total_seconds() < 0:
                    password_expired = True
                elif delta <= timedelta(days=15):
                    password_warning = True

            data.append({
                "username": username,
                "password": password,
                "permission": permission,
                "first_login": first_login,

                "password_created_at": password_created_at.strftime("%Y-%m-%d %H:%M:%S") if password_created_at else None,
                "password_expire_at": password_expire_at.strftime("%Y-%m-%d %H:%M:%S") if password_expire_at else None,
                "last_notify_at": last_notify_at.strftime("%Y-%m-%d %H:%M:%S") if last_notify_at else None,

                "password_warning": password_warning,
                "password_expired": password_expired,
                "password_days_left": password_days_left,
            })

        return jsonify({
            "ok": True,
            "count": len(data),
            "data": data
        })

    except Exception as e:
        return jsonify({
            "ok": False,
            "error": str(e)
        }), 500

# ================== 读取ini文件 ==================
INI_PATH = Path(os.environ.get("LC_INI_PATH", "/home/zlg/lc_data_set.ini"))

def get_ini_path() -> Path:
    """
    解析 ini 路径：优先使用环境变量/默认值；如不存在，回退到脚本同目录的 lc_data_set.ini。
    """
    if INI_PATH.is_file():
        return INI_PATH
    local_path = Path(__file__).with_name("lc_data_set.ini")
    if local_path.is_file():
        return local_path
    return INI_PATH

def read_ini_system_counts(path: Optional[Path] = None):
    path = Path(path) if path else get_ini_path()
    if not path.is_file():
        raise FileNotFoundError(f"ini 不存在: {path}")

    cfg = configparser.ConfigParser()
    cfg.optionxform = str  # 选项名保留大小写，便于与文件一致
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        cfg.read_file(f)

    sec = "SYSTEM" if "SYSTEM" in cfg else ("system" if "system" in cfg else None)
    if sec is None:
        raise KeyError("缺少 [SYSTEM] 段")

    def _get_int(name):
        if cfg.has_option(sec, name):
            return cfg.getint(sec, name)
        if cfg.has_option(sec, name.lower()):
            return cfg.getint(sec, name.lower())
        raise KeyError(f"[{sec}] 缺少字段: {name}")

    bms_num = _get_int("bmsNum")
    pcs_num = _get_int("pcsNum")
    return {"bmsNum": bms_num, "pcsNum": pcs_num}

def read_ini_all(path: Optional[Path] = None):
    path = Path(path) if path else get_ini_path()
    if not path.is_file():
        raise FileNotFoundError(f"ini 不存在: {path}")

    cfg = configparser.ConfigParser()
    cfg.optionxform = str
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        cfg.read_file(f)

    data = {}
    for sec in cfg.sections():
        data[sec] = {k: v for k, v in cfg.items(sec)}
    return data

@app.route("/ini/system_counts", methods=["GET", "POST"])
def ini_system_counts():
    try:
        data = read_ini_system_counts()
        resp = {"ok": True, "path": str(get_ini_path()), **data}
        print(f"[{now_str()}] [/ini/system_counts] 返回: {jdump(resp)}")
        return jsonify(resp)
    except FileNotFoundError as e:
        msg = {"ok": False, "error": str(e)}
        print(f"[{now_str()}] [/ini/system_counts] 404: {jdump(msg)}")
        return jsonify(msg), 404
    except (KeyError, ValueError) as e:
        msg = {"ok": False, "error": f"INI 字段缺失或格式错误: {e}"}
        print(f"[{now_str()}] [/ini/system_counts] 400: {jdump(msg)}")
        return jsonify(msg), 400
    except Exception as e:
        msg = {"ok": False, "error": f"服务器异常: {e}"}
        print(f"[{now_str()}] [/ini/system_counts] 500: {jdump(msg)}")
        return jsonify(msg), 500

@app.route("/ini/all", methods=["GET"])
def ini_all():
    try:
        data = read_ini_all()
        resp = {"ok": True, "path": str(get_ini_path()), "data": data}
        print(f"[{now_str()}] [/ini/all] 返回全部内容")
        return jsonify(resp)
    except FileNotFoundError as e:
        msg = {"ok": False, "error": str(e)}
        print(f"[{now_str()}] [/ini/all] 404: {jdump(msg)}")
        return jsonify(msg), 404
    except Exception as e:
        msg = {"ok": False, "error": f"服务器异常: {e}"}
        print(f"[{now_str()}] [/ini/all] 500: {jdump(msg)}")
        return jsonify(msg), 500

def update_ini_from_payload(payload: dict, path: Optional[Path] = None):
    """
    前端字段 -> lc_data_set.ini 映射：
    - mainSystem -> sysNum
    - subSystem -> subNum
    - pcsCount -> pcsNum
    - bmsCount -> bmsNum
    - pointToPointEnabled -> P2P_EN (bool -> 0/1)
    - pcsList/bmsList: id/ip/port -> pcs{id}_ip, pcs{id}_port / bms{id}_ip, bms{id}_port
    - testCtlList: id/ip/port -> measure{id}_ip, measure{id}_port
    - bmsBrand -> bms1_brand ~ bms8_brand
    - pcsBrand -> pcs1_brand ~ pcs4_brand
    其余字段保持不变。
    """
    path = Path(path) if path else get_ini_path()
    if not path.is_file():
        raise FileNotFoundError(f"ini 不存在: {path}")

    cfg = configparser.ConfigParser()
    cfg.optionxform = str
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        cfg.read_file(f)

    if not cfg.has_section("SYSTEM"):
        cfg.add_section("SYSTEM")
    if not cfg.has_section("PCS_NETWORK"):
        cfg.add_section("PCS_NETWORK")
    if not cfg.has_section("BMS_NETWORK"):
        cfg.add_section("BMS_NETWORK")
    if not cfg.has_section("MEASURE_NETWORK"):
        cfg.add_section("MEASURE_NETWORK")

    mapping = {
        "mainSystem": "sysNum",
        "subSystem": "subNum",
        "pcsCount": "pcsNum",
        "bmsCount": "bmsNum",
    }

    system_updates = {}
    for req_key, ini_key in mapping.items():
        if req_key in payload:
            try:
                val = int(payload[req_key])
            except (TypeError, ValueError):
                raise ValueError(f"{req_key} 需为整数")
            cfg.set("SYSTEM", ini_key, str(val))
            system_updates[ini_key] = val

    if "pointToPointEnabled" in payload:
        val_raw = payload["pointToPointEnabled"]
        val = 1 if str(val_raw).lower() in ("1", "true", "yes", "on") else 0
        cfg.set("SYSTEM", "P2P_EN", str(val))
        system_updates["P2P_EN"] = val

    def _update_devices(section: str, prefix: str, items, label: str):
        if not items:
            return []
        updated = []
        for item in items:
            if not isinstance(item, dict):
                raise ValueError(f"{label} 项需为object: {item}")
            try:
                idx = int(item["id"])
            except Exception:
                raise ValueError(f"{label} 项缺 id 或非整数: {item}")
            ip = str(item.get("ip", "")).strip()
            if not ip:
                raise ValueError(f"{label}{idx} 缺 ip")
            try:
                port = int(item.get("port"))
            except Exception:
                raise ValueError(f"{label}{idx} port 需为整数")
            cfg.set(section, f"{prefix}{idx}_ip", ip)
            cfg.set(section, f"{prefix}{idx}_port", str(port))
            updated.append({"id": idx, "ip": ip, "port": port})
        return updated

    pcs_updated = _update_devices("PCS_NETWORK", "pcs", payload.get("pcsList", []), "pcs")
    bms_updated = _update_devices("BMS_NETWORK", "bms", payload.get("bmsList", []), "bms")
    measure_updated = _update_devices("MEASURE_NETWORK", "measure", payload.get("testCtlList", []), "measure")

    brand_updates = {
        "bms_brand": [],
        "pcs_brand": [],
    }

    if "bmsBrand" in payload:
        try:
            bms_brand_val = int(payload["bmsBrand"])
        except (TypeError, ValueError):
            raise ValueError("bmsBrand 需为整数")

        for i in range(1, 9):
            key = f"bms{i}_brand"
            cfg.set("BMS_NETWORK", key, str(bms_brand_val))
            brand_updates["bms_brand"].append({key: bms_brand_val})

    if "pcsBrand" in payload:
        try:
            pcs_brand_val = int(payload["pcsBrand"])
        except (TypeError, ValueError):
            raise ValueError("pcsBrand 需为整数")

        for i in range(1, 5):
            key = f"pcs{i}_brand"
            cfg.set("PCS_NETWORK", key, str(pcs_brand_val))
            brand_updates["pcs_brand"].append({key: pcs_brand_val})

    with open(path, "w", encoding="utf-8") as f:
        cfg.write(f)

    return {
        "path": str(path),
        "system": system_updates,
        "pcs": pcs_updated,
        "bms": bms_updated,
        "measure": measure_updated,
        **brand_updates,
    }
def verify_ini_matches_payload(payload: dict, path: Optional[Path] = None):
    """
    读取写回后的 ini，与前端 payload 做一致性校对。只校对被写入的字段。
    """
    path = Path(path) if path else get_ini_path()
    if not path.is_file():
        return {"ok": False, "errors": [f"ini 不存在: {path}"]}

    cfg = configparser.ConfigParser()
    cfg.optionxform = str
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        cfg.read_file(f)

    errors = []
    sec = "SYSTEM"
    mapping = {
        "mainSystem": "sysNum",
        "subSystem": "subNum",
        "pcsCount": "pcsNum",
        "bmsCount": "bmsNum",
    }

    for req_key, ini_key in mapping.items():
        if req_key in payload:
            try:
                expected = int(payload[req_key])
                actual = cfg.getint(sec, ini_key, fallback=None)
                if actual is None or actual != expected:
                    errors.append(f"[SYSTEM]{ini_key}={actual} 期望 {expected}")
            except Exception as e:
                errors.append(f"[SYSTEM]{ini_key} 读取失败: {e}")

    if "pointToPointEnabled" in payload:
        expected = 1 if str(payload["pointToPointEnabled"]).lower() in ("1", "true", "yes", "on") else 0
        actual = cfg.getint(sec, "P2P_EN", fallback=None)
        if actual is None or actual != expected:
            errors.append(f"[SYSTEM]P2P_EN={actual} 期望 {expected}")

    def _check_devices(section: str, prefix: str, items, label: str):
        if not items:
            return
        for item in items:
            try:
                idx = int(item["id"])
            except Exception:
                errors.append(f"{label} 项缺少/非法 id: {item}")
                continue
            expected_ip = str(item.get("ip", "")).strip()
            expected_port = None
            try:
                expected_port = int(item.get("port"))
            except Exception:
                errors.append(f"{label}{idx} port 非整数: {item.get('port')}")
            actual_ip = cfg.get(section, f"{prefix}{idx}_ip", fallback=None)
            actual_port = cfg.getint(section, f"{prefix}{idx}_port", fallback=None)
            if actual_ip != expected_ip:
                errors.append(f"[{section}]{prefix}{idx}_ip={actual_ip} 期望 {expected_ip}")
            if expected_port is not None and actual_port != expected_port:
                errors.append(f"[{section}]{prefix}{idx}_port={actual_port} 期望 {expected_port}")

    _check_devices("PCS_NETWORK", "pcs", payload.get("pcsList", []), "pcs")
    _check_devices("BMS_NETWORK", "bms", payload.get("bmsList", []), "bms")
    _check_devices("MEASURE_NETWORK", "measure", payload.get("testCtlList", []), "measure")

    if "bmsBrand" in payload:
        try:
            expected_bms_brand = int(payload["bmsBrand"])
            for i in range(1, 9):
                actual = cfg.getint("BMS_NETWORK", f"bms{i}_brand", fallback=None)
                if actual is None or actual != expected_bms_brand:
                    errors.append(f"[BMS_NETWORK]bms{i}_brand={actual} 期望 {expected_bms_brand}")
        except Exception as e:
            errors.append(f"bmsBrand 校验失败: {e}")

    if "pcsBrand" in payload:
        try:
            expected_pcs_brand = int(payload["pcsBrand"])
            for i in range(1, 5):
                actual = cfg.getint("PCS_NETWORK", f"pcs{i}_brand", fallback=None)
                if actual is None or actual != expected_pcs_brand:
                    errors.append(f"[PCS_NETWORK]pcs{i}_brand={actual} 期望 {expected_pcs_brand}")
        except Exception as e:
            errors.append(f"pcsBrand 校验失败: {e}")

    return {"ok": len(errors) == 0, "errors": errors}
def reboot_system():
    """
    调用系统 reboot。若失败，返回错误字符串；成功/已发起返回 None。
    """
    try:
        result = subprocess.run(["reboot"], capture_output=True, text=True)
        if result.returncode != 0:
            return f"reboot 失败: rc={result.returncode}, stderr={result.stderr.strip()}"
        return None
    except Exception as e:
        return f"reboot 异常: {e}"

def schedule_reboot(delay_sec: int = 2):
    """
    异步延时重启，避免阻塞响应。
    """
    def _task():
        err = reboot_system()
        if err:
            print(f"[{now_str()}] [REBOOT] 重启失败: {err}")
        else:
            print(f"[{now_str()}] [REBOOT] 已触发系统重启")

    try:
        t = threading.Timer(delay_sec, _task)
        t.daemon = True
        t.start()
        return None
    except Exception as e:
        return f"调度重启异常: {e}"
# ================== 重启标志循环检测 ==================
REBOOT_FLAG_ADDR = 1399
REBOOT_FLAG_VALUE = 0xAA
REBOOT_FLAG_CLEAR_VALUE = 0
REBOOT_FLAG_CHECK_INTERVAL = 1.0  # 秒，可改成 0.2 或 2.0

reboot_flag_triggered = threading.Event()


def check_reboot_flag_once():
    """
    单次检测重启标志：
    - 读取保持寄存器 1399
    - 如果值为 0xAA，先清零
    - 清零成功后调度系统重启
    """
    if reboot_flag_triggered.is_set():
        return False

    try:
        flag_value = int(registers_model.get_hold(REBOOT_FLAG_ADDR))
    except Exception as e:
        print(f"[{now_str()}] [REBOOT_FLAG] 读取寄存器{REBOOT_FLAG_ADDR}异常: {e}")
        return False

    if flag_value != REBOOT_FLAG_VALUE:
        return False

    print(
        f"[{now_str()}] [REBOOT_FLAG] 检测到重启条件满足: "
        f"hold[{REBOOT_FLAG_ADDR}]=0x{flag_value:02X}"
    )

    try:
        result, attempt_no = write_register_with_retry(
            REBOOT_FLAG_ADDR,
            REBOOT_FLAG_CLEAR_VALUE,
            slave=1
        )

        if result is None or result.isError():
            print(
                f"[{now_str()}] [REBOOT_FLAG] 清零寄存器失败: "
                f"addr={REBOOT_FLAG_ADDR}, result={result}, attempt={attempt_no}"
            )
            return False

        print(
            f"[{now_str()}] [REBOOT_FLAG] 已清零寄存器: "
            f"addr={REBOOT_FLAG_ADDR}, value={REBOOT_FLAG_CLEAR_VALUE}, attempt={attempt_no}"
        )

        reboot_flag_triggered.set()

        reboot_err = schedule_reboot(2)
        if reboot_err is not None:
            reboot_flag_triggered.clear()
            print(f"[{now_str()}] [REBOOT_FLAG] 调度重启失败: {reboot_err}")
            return False

        print(f"[{now_str()}] [REBOOT_FLAG] 已调度 2 秒后系统重启")
        return True

    except Exception as e:
        print(f"[{now_str()}] [REBOOT_FLAG] 清零或重启处理异常: {e}")

        try:
            with remote_client_lock:
                remote_client.close()
        except Exception:
            pass

        return False


def reboot_flag_monitor_loop():
    """
    后台循环检测重启标志。
    """
    print(
        f"[{now_str()}] [REBOOT_FLAG] 重启标志检测线程已启动: "
        f"addr={REBOOT_FLAG_ADDR}, target=0x{REBOOT_FLAG_VALUE:02X}, "
        f"interval={REBOOT_FLAG_CHECK_INTERVAL}s"
    )

    while True:
        check_reboot_flag_once()
        time.sleep(REBOOT_FLAG_CHECK_INTERVAL)


def start_reboot_flag_monitor():
    """
    启动后台检测线程。
    """
    t = threading.Thread(
        target=reboot_flag_monitor_loop,
        name="reboot_flag_monitor",
        daemon=True
    )
    t.start()
    return t
# ================== 网络配置接口 ==================
NETWORK_CONFIG_PATH = Path("/etc/network/interfaces")

def normalize_interface_name(raw_iface: str) -> str:
    """
    将传入的网口号/名称标准化：
    - 纯数字 -> eth{数字}
    - ethX 形式转为小写
    - 其余仅允许字母/数字/._:- 组合
    """
    if raw_iface is None:
        raise ValueError("缺少网口号/名称")
    iface = str(raw_iface).strip()
    if not iface:
        raise ValueError("网口号/名称为空")
    if iface.isdigit():
        return f"eth{iface}"
    if iface.lower().startswith("eth") and iface[3:].isdigit():
        return f"eth{int(iface[3:])}"
    if not re.fullmatch(r"[A-Za-z0-9_.:-]+", iface):
        raise ValueError("网口名称仅支持字母/数字/._:-")
    return iface

def parse_ip_interface(ip_value, netmask=None, prefix=None) -> ipaddress.IPv4Interface:
    """
    解析 IP，支持：
    - 直接传 CIDR，如 192.168.1.10/24
    - 传 ip + netmask（255.255.255.0 或前缀数字）
    - 传 ip + prefix（前缀数字）
    """
    if ip_value is None:
        raise ValueError("缺少 IP 地址")
    ip_text = str(ip_value).strip()
    if not ip_text:
        raise ValueError("IP 地址为空")

    try:
        if "/" in ip_text:
            ip_iface = ipaddress.ip_interface(ip_text)
        else:
            if prefix is not None and str(prefix).strip():
                ip_iface = ipaddress.ip_interface(f"{ip_text}/{int(prefix)}")
            elif netmask:
                mask_raw = str(netmask).strip()
                if mask_raw.isdigit():
                    ip_iface = ipaddress.ip_interface(f"{ip_text}/{int(mask_raw)}")
                else:
                    try:
                        prefix_len = ipaddress.IPv4Network(f"0.0.0.0/{mask_raw}", strict=False).prefixlen
                    except Exception:
                        raise ValueError(f"无效的子网掩码: {mask_raw}")
                    ip_iface = ipaddress.ip_interface(f"{ip_text}/{prefix_len}")
            else:
                raise ValueError("IP 需带掩码，如 192.168.1.10/24，或提供 netmask/prefix")
    except ValueError as e:
        raise ValueError(f"IP 地址格式错误: {e}")

    if ip_iface.version != 4:
        raise ValueError("仅支持 IPv4")
    return ip_iface

def apply_ip_runtime(iface: str, ip_iface: ipaddress.IPv4Interface) -> Optional[str]:
    """
    立即更新网口 IP（仅本次运行，需持久化配置文件保证重启后生效）。
    返回 None 表示成功，返回字符串表示错误。
    """
    cmds = [
        (["ip", "addr", "flush", "dev", iface], "flush"),
        (["ip", "addr", "add", str(ip_iface), "dev", iface], "add"),
        (["ip", "link", "set", iface, "up"], "link_up"),
    ]
    for cmd, desc in cmds:
        rc, so, se = run_cmd(cmd, timeout=10)
        if rc != 0:
            return f"{desc} 失败(rc={rc}): {(se or so).strip()}"
    return None

def apply_gateway_runtime(iface: str, gateway: ipaddress.IPv4Address) -> Optional[str]:
    """
    设置默认网关（替换当前默认路由）。
    """
    rc, so, se = run_cmd(["ip", "route", "replace", "default", "via", str(gateway), "dev", iface], timeout=10)
    if rc != 0:
        return f"设置网关失败(rc={rc}): {(se or so).strip()}"
    return None

def persist_interface_config(iface: str, ip_iface: ipaddress.IPv4Interface,
                             path: Path = NETWORK_CONFIG_PATH,
                             gateway: Optional[ipaddress.IPv4Address] = None) -> Optional[str]:
    """
    将网口 IP 写入 /etc/network/interfaces，覆盖原有 iface 块并追加新的静态配置。
    返回 None 表示成功，返回字符串表示错误。
    """
    if not path.exists():
        return f"持久化失败: {path} 不存在"
    try:
        lines = path.read_text().splitlines()
    except Exception as e:
        return f"读取 {path} 失败: {e}"

    new_lines = []
    i = 0
    while i < len(lines):
        line = lines[i]
        stripped = line.strip()
        if stripped.startswith("iface") and stripped.split()[1:2] == [iface]:
            i += 1
            while i < len(lines):
                nxt = lines[i]
                if nxt.startswith(" ") or not nxt.strip():
                    i += 1
                    continue
                break
            continue
        new_lines.append(line)
        i += 1

    if not any(l.strip().startswith(f"auto {iface}") for l in new_lines):
        new_lines.append(f"auto {iface}")

    new_lines.extend([
        f"iface {iface} inet static",
        f"    address {ip_iface.ip}",
        f"    netmask {ip_iface.netmask}",
    ])
    if gateway:
        new_lines.append(f"    gateway {gateway}")
    new_lines.append("")

    try:
        path.write_text("\n".join(new_lines).rstrip() + "\n")
        return None
    except Exception as e:
        return f"写入 {path} 失败: {e}"

def persist_network_to_ini(iface: str, ip_iface: ipaddress.IPv4Interface,
                           gateway: Optional[ipaddress.IPv4Address] = None,
                           path: Path = INI_PATH) -> Tuple[str, Optional[str]]:
    """
    将 /network/set_ip 的新配置写入 lc_data_set.ini。
    若已存在相同 iface 则替换，否则新增一个 NETWORK_* 段。
    返回 (写入路径, 错误或 None)。
    """
    path = Path(path)
    cfg = configparser.ConfigParser()
    cfg.optionxform = str

    seed_path = path if path.is_file() else get_ini_path()
    if seed_path.is_file():
        try:
            with open(seed_path, "r", encoding="utf-8", errors="ignore") as f:
                cfg.read_file(f)
        except Exception as e:
            return str(path), f"读取 {seed_path} 失败: {e}"

    section = None
    for sec in cfg.sections():
        if sec == "NETWORK" or sec.startswith("NETWORK_") or sec.startswith("NETWORK:"):
            if cfg.get(sec, "iface", fallback=None) == iface:
                section = sec
                break
    if section is None:
        if cfg.has_section("NETWORK") and not cfg.has_option("NETWORK", "iface"):
            section = "NETWORK"
        else:
            section = f"NETWORK_{iface}"
        if not cfg.has_section(section):
            cfg.add_section(section)

    cfg.set(section, "iface", iface)
    cfg.set(section, "cidr", str(ip_iface))
    cfg.set(section, "ip", str(ip_iface.ip))
    cfg.set(section, "netmask", str(ip_iface.netmask))
    cfg.set(section, "prefix", str(ip_iface.network.prefixlen))
    if gateway:
        cfg.set(section, "gateway", str(gateway))
    elif cfg.has_option(section, "gateway"):
        cfg.remove_option(section, "gateway")

    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            cfg.write(f)
        return str(path), None
    except Exception as e:
        return str(path), f"写入 {path} 失败: {e}"

def schedule_network_change(iface: str, ip_iface: ipaddress.IPv4Interface,
                            gateway: Optional[ipaddress.IPv4Address] = None,
                            delay_sec: float = 0.5) -> Optional[str]:
    """
    后台异步修改 IP/网关并持久化，避免阻断当前 HTTP 连接。
    """
    def _task():
        try:
            if delay_sec > 0:
                time.sleep(delay_sec)
            apply_err = apply_ip_runtime(iface, ip_iface)
            if apply_err:
                print(f"[{now_str()}] [/network/set_ip] 后台应用失败: {apply_err}")
                return
            if gateway:
                gw_err = apply_gateway_runtime(iface, gateway)
                if gw_err:
                    print(f"[{now_str()}] [/network/set_ip] 后台网关失败: {gw_err}")
                    return
            persist_err = persist_interface_config(iface, ip_iface, gateway=gateway)
            reboot_err = None
            if persist_err is None:
                reboot_err = schedule_reboot(2)
            else:
                reboot_err = "未调度（持久化失败）"
            print(f"[{now_str()}] [/network/set_ip] 后台完成 iface={iface}, ip={ip_iface}, gateway={gateway}, "
                  f"persist_err={persist_err}, reboot_err={reboot_err}")
        except Exception as e:
            print(f"[{now_str()}] [/network/set_ip] 后台任务异常: {e}")

    try:
        t = threading.Thread(target=_task, daemon=True)
        t.start()
        return None
    except Exception as e:
        return f"调度网络变更失败: {e}"

@app.route("/network/set_ip", methods=["POST"])
def network_set_ip():
    """
    支持两种请求格式：

    1. 单对象：
    {
        "iface": "net1",
        "ip": "192.168.2.136",
        "netmask": "255.255.255.0",
        "gateway": "192.168.2.1"
    }

    2. 数组：
    [
        {
            "id": 1,
            "iface": "net1",
            "ip": "192.168.2.136",
            "netmask": "255.255.255.0",
            "gateway": "192.168.2.1"
        },
        {
            "id": 2,
            "iface": "net2",
            "ip": "192.168.2.137",
            "netmask": "255.255.255.0",
            "gateway": "192.168.2.1"
        }
    ]

    说明：
      - iface 仅允许 net1/net2/net3/net4
      - 只允许 net1 设置网关，其他接口只支持设置 IP 和子网掩码
      - 支持一次修改 1~4 个端口
      - 如果任何一个端口参数非法，则整个请求失败，不执行任何修改
      - 所有校验通过后，统一调度后台应用 IP，并写入 ini
    """
    try:
        payload = request.get_json(force=True)
    except Exception as e:
        msg = {"ok": False, "error": f"无法解析 JSON: {e}"}
        print(f"[{now_str()}] [/network/set_ip] 400 JSON: {jdump(msg)}")
        return jsonify(msg), 400

    if not payload:
        msg = {"ok": False, "error": "请求体为空"}
        print(f"[{now_str()}] [/network/set_ip] 400 空请求: {jdump(msg)}")
        return jsonify(msg), 400

    # 兼容单对象和数组
    if isinstance(payload, dict):
        items = [payload]
    elif isinstance(payload, list):
        items = payload
    else:
        msg = {"ok": False, "error": "请求格式错误，必须为对象或对象数组"}
        print(f"[{now_str()}] [/network/set_ip] 400 格式错误: {jdump(msg)}")
        return jsonify(msg), 400

    if len(items) == 0:
        msg = {"ok": False, "error": "请求数组不能为空"}
        print(f"[{now_str()}] [/network/set_ip] 400 空数组: {jdump(msg)}")
        return jsonify(msg), 400

    allowed_ifaces = {"net1", "net2", "net3", "net4"}
    parsed_items = []
    req_ip_set = set()

    # =========================
    # 第一阶段：全部校验，不做任何修改
    # =========================
    for idx, item in enumerate(items):
        if not isinstance(item, dict):
            msg = {"ok": False, "error": f"第 {idx+1} 项不是对象"}
            print(f"[{now_str()}] [/network/set_ip] 400 项格式错误: {jdump(msg)}")
            return jsonify(msg), 400

        raw_iface = item.get("iface") or item.get("interface") or item.get("port")
        raw_ip = item.get("ip") or item.get("address")
        netmask = item.get("netmask") or item.get("mask")
        prefix = item.get("prefix")
        gateway_raw = item.get("gateway") or item.get("gw")
        item_id = item.get("id", idx + 1)

        if raw_iface is None or raw_ip is None:
            msg = {
                "ok": False,
                "error": f"第 {idx+1} 项缺少 iface/interface/port 或 ip/address 字段",
                "item": item
            }
            print(f"[{now_str()}] [/network/set_ip] 400 缺字段: {jdump(msg)}")
            return jsonify(msg), 400

        try:
            iface = normalize_interface_name(raw_iface).strip()
        except Exception as e:
            msg = {
                "ok": False,
                "error": f"第 {idx+1} 项接口名非法: {e}",
                "item": item
            }
            print(f"[{now_str()}] [/network/set_ip] 400 iface错误: {jdump(msg)}")
            return jsonify(msg), 400

        if iface not in allowed_ifaces:
            msg = {
                "ok": False,
                "error": f"第 {idx+1} 项接口 {iface} 不允许，必须是 net1/net2/net3/net4",
                "item": item
            }
            print(f"[{now_str()}] [/network/set_ip] 400 iface范围错误: {jdump(msg)}")
            return jsonify(msg), 400

        try:
            ip_iface = parse_ip_interface(raw_ip, netmask=netmask, prefix=prefix)

            gateway_ip = None
            # 只有 net1 可以设置网关，其他接口直接忽略，但仍然记录到 INI 文件中
            if gateway_raw and iface == "net1":
                gateway_ip = ipaddress.ip_address(str(gateway_raw).strip())
                if gateway_ip.version != 4:
                    raise ValueError("仅支持 IPv4 网关")

                if gateway_ip not in ip_iface.network:
                    raise ValueError(
                        f"The gateway {gateway_ip} and the IP address {ip_iface} "
                        f"are not on the same network segment ({ip_iface.network})"
                    )

                if gateway_ip == ip_iface.ip:
                    raise ValueError("The gateway cannot be the same as the local IP address.")
            elif gateway_raw and iface != "net1":
                # 对于 net2、net3、net4，网关不生效，但仍然写入 INI 文件
                gateway_ip = ipaddress.ip_address(str(gateway_raw).strip()) if gateway_raw else None
        except ValueError as e:
            msg = {
                "ok": False,
                "error": f"第 {idx+1} 项参数校验失败: {e}",
                "item": item
            }
            print(f"[{now_str()}] [/network/set_ip] 400 IP校验失败: {jdump(msg)}")
            return jsonify(msg), 400

        # 检查本次请求中是否有重复 IP
        ip_str = str(ip_iface.ip)
        if ip_str in req_ip_set:
            msg = {
                "ok": False,
                "error": f"请求中存在重复 IP: {ip_str}",
                "item": item
            }
            print(f"[{now_str()}] [/network/set_ip] 400 重复IP: {jdump(msg)}")
            return jsonify(msg), 400
        req_ip_set.add(ip_str)

        # 检查接口是否存在
        rc, so, se = run_cmd(["ip", "link", "show", iface], timeout=5)
        if rc != 0:
            msg = {
                "ok": False,
                "error": f"网口不存在: {iface}",
                "stdout": so.strip(),
                "stderr": se.strip(),
                "item": item
            }
            print(f"[{now_str()}] [/network/set_ip] 404 网口不存在: {jdump(msg)}")
            return jsonify(msg), 404

        parsed_items.append({
            "id": item_id,
            "iface": iface,
            "ip_iface": ip_iface,
            "gateway_ip": gateway_ip,
            "raw_item": item
        })

    # =========================
    # 第二阶段：全部执行
    # =========================
    results = []
    errors = []

    for entry in parsed_items:
        iface = entry["iface"]
        ip_iface = entry["ip_iface"]
        gateway_ip = entry["gateway_ip"]
        item_id = entry["id"]

        apply_delay = 0.5
        schedule_err = schedule_network_change(
            iface,
            ip_iface,
            gateway=gateway_ip,
            delay_sec=apply_delay
        )

        ini_path, ini_err = persist_network_to_ini(
            iface,
            ip_iface,
            gateway=gateway_ip
        )

        one = {
            "id": item_id,
            "iface": iface,
            "cidr": str(ip_iface),
            "ip": str(ip_iface.ip),
            "netmask": str(ip_iface.netmask),
            "gateway": str(gateway_ip) if gateway_ip else None,
            "config_path": str(NETWORK_CONFIG_PATH),
            "ini_path": ini_path,
            "schedule_error": schedule_err,
            "ini_error": ini_err,
            "ok": (schedule_err is None and ini_err is None)
        }

        if one["ok"]:
            results.append(one)
        else:
            errors.append(one)

    resp = {
        "ok": len(errors) == 0,
        "count": len(parsed_items),
        "success_count": len(results),
        "error_count": len(errors),
        "results": results,
        "errors": errors
    }

    status = 200 if resp["ok"] else 500
    print(f"[{now_str()}] [/network/set_ip] 批量处理完成: {jdump(resp)}")
    return jsonify(resp), status

@app.route("/ini/update", methods=["POST"])
def ini_update():
    try:
        payload = request.get_json(force=True) or {}
    except Exception as e:
        msg = {"ok": False, "error": f"无法解析 JSON: {e}"}
        print(f"[{now_str()}] [/ini/update] 400 JSON: {jdump(msg)}")
        return jsonify(msg), 400

    try:
        result = update_ini_from_payload(payload)
        verify = verify_ini_matches_payload(payload, result["path"])
        if not verify["ok"]:
            msg = {"ok": False, "error": "INI 校验失败", "details": verify["errors"], "path": result["path"]}
            print(f"[{now_str()}] [/ini/update] 校验失败: {jdump(msg)}")
            return jsonify(msg), 400

        reboot_err = schedule_reboot(2)
        resp = {
            "ok": reboot_err is None,
            **result,
            "verify": verify,
            "reboot_error": reboot_err,
            "reboot_scheduled_in": 2,
        }
        log_msg = "成功更新，已调度 2 秒后重启" if reboot_err is None else f"更新成功但调度重启失败: {reboot_err}"
        print(f"[{now_str()}] [/ini/update] {log_msg}: {jdump(resp)}")
        status = 200 if reboot_err is None else 500
        return jsonify(resp), status
    except FileNotFoundError as e:
        msg = {"ok": False, "error": str(e)}
        print(f"[{now_str()}] [/ini/update] 404: {jdump(msg)}")
        return jsonify(msg), 404
    except ValueError as e:
        msg = {"ok": False, "error": str(e)}
        print(f"[{now_str()}] [/ini/update] 400: {jdump(msg)}")
        return jsonify(msg), 400
    except Exception as e:
        msg = {"ok": False, "error": f"更新失败: {e}"}
        print(f"[{now_str()}] [/ini/update] 500: {jdump(msg)}")
        return jsonify(msg), 500
@app.route("/usb/status", methods=["GET"])
def usb_status():
    """
    查询 USB 当前启用/禁用状态
    """
    ok, err, status = read_usb_status()
    if not ok:
        return jsonify({
            "ok": False,
            "error": err
        }), 500

    mount_ok, mount_err, mounted_list = get_mounted_usb_partitions()

    return jsonify({
        "ok": True,
        "usb": status,
        "mounted_usb": mounted_list if mount_ok else [],
        "mount_error": mount_err if not mount_ok else ""
    })


@app.route("/usb/control", methods=["POST"])
def usb_control():
    """
    控制 USB 启用/禁用。

    请求示例：
    {
        "action": "on"
    }

    或：
    {
        "action": "off"
    }

    可选：
    {
        "action": "off",
        "force": true
    }

    force = true 表示即使 umount 失败，也强制禁用 USB。
    一般不建议前端默认传 true。
    """
    data = request.get_json(force=True) or {}

    action = str(data.get("action", "")).strip().lower()
    force = bool(data.get("force", False))

    if action in ["on", "enable", "enabled", "1", "true"]:
        enable = True

    elif action in ["off", "disable", "disabled", "0", "false"]:
        enable = False

    else:
        return jsonify({
            "ok": False,
            "error": "参数错误，action 只能是 on/off 或 enable/disable"
        }), 400

    # 禁用前先安全卸载 U 盘
    umount_info = None

    if not enable:
        umount_ok, umount_err, umount_info = safe_umount_usb_partitions()

        if not umount_ok and not force:
            return jsonify({
                "ok": False,
                "error": "USB 禁用失败：存在 U 盘分区卸载失败，为避免文件系统损坏，已取消禁用",
                "detail": umount_err,
                "umount_info": umount_info,
                "suggestion": "请确认没有程序正在访问U盘文件，或请求时传入 force=true 强制禁用"
            }), 409

    ok, err = write_usb_status(enable)
    if not ok:
        return jsonify({
            "ok": False,
            "error": err
        }), 500

    status_ok, status_err, status = read_usb_status()

    return jsonify({
        "ok": True,
        "message": "USB 已启用" if enable else "USB 已禁用",
        "usb": status if status_ok else None,
        "umount_info": umount_info
    })
@app.route("/", defaults={"path": ""})
@app.route("/<path:path>")
def serve_spa(path):
    if path.startswith("api"):
        return jsonify({"error": "not found"}), 404
    return app.send_static_file("index.html")

# ================== 后台轮询线程 ==================
def read_remote_modbus():
    try:
        if not remote_client.is_socket_open():
            print(f"[{now_str()}] [REMOTE] socket 未开启，跳过读取")
            return
        with remote_client_lock:
            result = remote_client.read_input_registers(address=30000, count=10, slave=1)
        if result.isError():
            print(f"[{now_str()}] [REMOTE] 读取失败: {result}")
        else:
            values = getattr(result, "registers", [])[:5]
            # 如需使用 values，可在此处处理
    except Exception as e:
        print(f"[{now_str()}] [REMOTE] Modbus 异常: {e}")
        with remote_client_lock:
            remote_client.close()

def background_thread():
    print(f"[{now_str()}] [BACKGROUND THREAD] 启动后台线程")
    while True:
        read_remote_modbus()
        socketio.emit('refreshData', [])
        print(f"[{now_str()}] [SocketIO] emit refreshData []")
        time.sleep(1)

@socketio.on('connect')
def on_connect():
    print(f"[{now_str()}] [SocketIO] 新客户端连接: {request.remote_addr}")

def main():
    print(f"[{now_str()}] [MAIN] 尝试首次连接远程 Modbus 服务器 {HOST_IP}:1502")
    with remote_client_lock:
        ok = ensure_remote_connected("startup")
    if not ok:
        print(f"[{now_str()}] [MAIN] 无法连接远程 Modbus TCP 服务器")
    else:
        print(f"[{now_str()}] [MAIN] 成功连接远程 Modbus TCP 服务器")
    socketio.start_background_task(background_thread)
    print(f"[{now_str()}] [MAIN] 启动 Flask SocketIO 服务器 (0.0.0.0:8000)")
    start_reboot_flag_monitor()
    socketio.run(app, host='0.0.0.0', port=8000, debug=False, use_reloader=False, allow_unsafe_werkzeug=True)

if __name__ == '__main__':
    main()
