from tkinter import filedialog
import os

class MainController:
    def __init__(self, root, view, model):
        self.root = root
        self.view = view
        self.model = model
        
        # State Machine
        self.selected_filepath = None
        self.state = "NO_FILE"  # States: NO_FILE, READY_TO_LOCK, READY_TO_UNLOCK, WAITING_FOR_BT

        self.view.bind_connect_button(self.handle_connect_toggle)
        self.view.bind_select_file_button(self.handle_select_file)
        self.view.bind_lock_button(self.handle_action_button) 

        self.model.set_callbacks(
            on_log=self.update_log,
            on_status_change=self.update_status,
            on_message=self.handle_bt_message 
        )

    # Bluetooth Message Handler
    def handle_bt_message(self, msg):
        clean_msg = msg.strip().upper() 
        
        # If waiting for the password
        if self.state == "WAITING_FOR_BT":
            if clean_msg == "UNLOCK": # If Received Message
                self.update_log("[SUCCESS] Message Received! Decrypting file...")
                self.process_file_decryption()
            else:
                self.update_log(f"[DENIED] Wrong message received: {clean_msg}")
        else:
            # If not trying to unlock, only log message
            self.update_log(f"[MSG] {msg}")

    def handle_connect_toggle(self):
        if self.model.is_running:
            self.model.stop_server()
        else:
            self.model.start_server()

    def handle_select_file(self):
        filepath = filedialog.askopenfilename(title="Select a File")
        if not filepath:
            return 

        self.selected_filepath = filepath
        filename = os.path.basename(filepath)
        self.view.update_selected_file(filename)

        # State Treatment: Is it already encrypted?
        if filename.endswith(".locked"):
            self.state = "READY_TO_UNLOCK"
            self.view.update_action_button("Unlock File")
            self.update_log(f"[*] Loaded encrypted file: {filename}")
        else:
            self.state = "READY_TO_LOCK"
            self.view.update_action_button("Lock File")
            self.update_log(f"[*] Loaded normal file: {filename}")

    def handle_action_button(self):
        # State Treatment: Is a file selected?
        if self.state == "NO_FILE":
            self.update_log("[!] Error: You must select a file first!")
            return

        # State Treatment: Lock the file
        if self.state == "READY_TO_LOCK":
            self.update_log("[*] Encrypting file...")
            success, new_path = self.model.encrypt_file(self.selected_filepath)
            
            if success:
                self.selected_filepath = new_path
                self.state = "READY_TO_UNLOCK"
                self.view.update_selected_file(os.path.basename(new_path))
                self.view.update_action_button("Unlock File")
                self.update_log("[+] File securely locked.")

        # State Treatment: Transition to waiting for Bluetooth
        elif self.state == "READY_TO_UNLOCK":
            if not self.model.is_running:
                self.update_log("[!] Error: Connect the Bluetooth server first to receive the password!")
                return
                
            self.state = "WAITING_FOR_BT"
            self.view.update_action_button("Waiting for BT...")
            self.update_log("[*] System ready. Waiting for fingerprint validation...")

        # State Treatment: Cancel the waiting state
        elif self.state == "WAITING_FOR_BT":
            self.state = "READY_TO_UNLOCK"
            self.view.update_action_button("Unlock File")
            self.update_log("[-] Cancelled waiting state.")

    def process_file_decryption(self):
        success, new_path = self.model.decrypt_file(self.selected_filepath)
        if success:
            self.selected_filepath = new_path
            self.state = "READY_TO_LOCK"
            self.view.update_selected_file(os.path.basename(new_path))
            self.view.update_action_button("Lock File")
            self.update_log("[+] File successfully unlocked and restored!")

    def update_log(self, message):
        self.root.after(0, self.view.append_log, message)

    def update_status(self, text, is_connected):
        self.root.after(0, self.view.update_connection_status, text, is_connected)