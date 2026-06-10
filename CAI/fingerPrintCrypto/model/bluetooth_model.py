import socket
import subprocess
import threading
import time
import os
import re

class BluetoothModel:
    def __init__(self):
        self.server_sock = None
        self.client_sock = None
        self.is_running = False
        self.mac_address = self._fetch_local_mac()
        self.port = 1
        
        # Byte key for XOR Cypher
        self.encryption_key = 170 
        
        self.on_log = lambda msg: None
        self.on_status_change = lambda msg, connected: None
        self.on_message = lambda msg: None 

    def _fetch_local_mac(self):
        try:
            # Run bluetoothctl show in background
            result = subprocess.run(['bluetoothctl', 'show'], capture_output=True, text=True, check=True)
            # Captuer controller mac-addr
            match = re.search(r"Controller ([0-9A-F]{2}:[0-9A-F]{2}:[0-9A-F]{2}:[0-9A-F]{2}:[0-9A-F]{2}:[0-9A-F]{2})", result.stdout, re.IGNORECASE)
            
            if match:
                return match.group(1).upper()
            else:
                return "" 
                
        except Exception as e:
            print(f"[Warning] Could not auto-fetch MAC: {e}")
            return ""

    def set_callbacks(self, on_log, on_status_change, on_message):
        self.on_log = on_log
        self.on_status_change = on_status_change
        self.on_message = on_message 

    def _setup_signpost(self):
        try:
            subprocess.run(['sdptool', 'add', 'SP'], check=True, capture_output=True)
            self.on_log("[System] SDP signpost added.")
        except subprocess.CalledProcessError:
            self.on_log("[Error] Failed to add signpost. Are you running with sudo?")

    def start_server(self):
        if self.is_running: return
        self.is_running = True
        self.on_status_change("Starting...", True)
        
        # Run socket code in a separate thread
        thread = threading.Thread(target=self._server_loop, daemon=True)
        thread.start()

    def stop_server(self):
        self.is_running = False
        
        if self.client_sock:
            try:
                self.client_sock.close()
            except Exception:
                pass
            self.client_sock = None

        if self.server_sock:
            try:
                self.server_sock.close()
            except Exception:
                pass
            self.server_sock = None

        self.on_log("[-] Server stopped.")
        self.on_status_change("Disconnected", False)

    def _server_loop(self):
        self._setup_signpost()
        time.sleep(1)
        self.server_sock = socket.socket(socket.AF_BLUETOOTH, socket.SOCK_STREAM, socket.BTPROTO_RFCOMM)
        
        try:
            self.server_sock.bind((self.mac_address, self.port))
            self.server_sock.listen(1)
            
            # Timeout so the thread does not execute indefinetely
            self.server_sock.settimeout(1.0)
            
            self.on_log(f"[+] Listening on {self.mac_address} (Ch {self.port})...")
            self.on_status_change("Listening...", True)

            while self.is_running:
                try:
                    self.client_sock, client_info = self.server_sock.accept()
                    self.on_log(f"\n[+] Client connected: {client_info}")
                    self.on_status_change("Connected", True)

                    self.client_sock.settimeout(1.0)

                    while self.is_running:
                        try:
                            data = self.client_sock.recv(1024)
                            if not data:
                                self.on_log("[-] Client disconnected cleanly.")
                                break
                            
                            raw_msg = data.decode('utf-8')
                            self.on_message(raw_msg)
                            
                        except socket.timeout:
                            continue
                        except ConnectionResetError:
                            self.on_log("[-] Client abruptly dropped the connection.")
                            break
                        except OSError:
                            break 

                except socket.timeout:
                    continue
                except OSError:
                    break
                finally:
                    if self.client_sock:
                        try:
                            self.client_sock.close()
                        except Exception: pass
                        self.client_sock = None
                        
                    if self.is_running:
                        self.on_status_change("Listening...", True)

        except Exception as e:
            self.on_log(f"[Error] {e}")
        finally:
            if self.server_sock:
                try:
                    self.server_sock.close()
                except Exception: pass
                self.server_sock = None
            self.is_running = False

    def encrypt_file(self, filepath):
        try:
            with open(filepath, 'rb') as file:
                data = bytearray(file.read())
            
            for i in range(len(data)):
                data[i] ^= self.encryption_key
                
            new_filepath = filepath + ".locked"
            with open(new_filepath, 'wb') as file:
                file.write(data)
                
            os.remove(filepath) 
            return True, new_filepath
        except Exception as e:
            self.on_log(f"[Error] Encryption failed: {e}")
            return False, filepath

    def decrypt_file(self, filepath):
        try:
            with open(filepath, 'rb') as file:
                data = bytearray(file.read())
            
            for i in range(len(data)):
                data[i] ^= self.encryption_key
                
            new_filepath = filepath.replace(".locked", "")
            with open(new_filepath, 'wb') as file:
                file.write(data)
                
            os.remove(filepath) 
            return True, new_filepath
        except Exception as e:
            self.on_log(f"[Error] Decryption failed: {e}")
            return False, filepath