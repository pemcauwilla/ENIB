import tkinter as tk
from tkinter import ttk
from tkinter import scrolledtext

class MainView(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self.parent = parent
        
        self.setup_style()
        self.setup_ui()
        self.setup_layout()

    def setup_style(self):
        style = ttk.Style(self)
        style.theme_use('clam') 

        self.bg_color = "#1A1A24"     
        self.text_color = "#F0F0F2"    
        self.cyan_accent = "#00D2D3"   
        self.pink_accent = "#FF477E"   
        self.log_bg = "#252530"        
        self.log_border = "#3A3A4A"   

        style.configure('TFrame', background=self.bg_color)
        style.configure('TLabel', background=self.bg_color, foreground=self.text_color, font=("Helvetica", 10))

        style.configure('Connect.TButton', 
                        background=self.cyan_accent, 
                        foreground="#1E1E24", 
                        font=("Helvetica", 10, "bold"), 
                        borderwidth=0,
                        anchor="center")
        style.map('Connect.TButton', background=[('active', '#00B8B8')])

        style.configure('Lock.TButton', 
                        background=self.pink_accent, 
                        foreground="#FFFFFF", 
                        font=("Helvetica", 10, "bold"), 
                        borderwidth=0,
                        anchor="center")
        style.map('Lock.TButton', background=[('active', '#E63E70')])

        style.configure('Standard.TButton', 
                        background=self.log_bg, 
                        foreground=self.text_color, 
                        font=("Helvetica", 10), 
                        borderwidth=1, 
                        bordercolor=self.log_border,
                        anchor="center")
        style.map('Standard.TButton', background=[('active', '#3A3A4A')])

    def setup_ui(self):
        self.conn_frame = ttk.Frame(self)
        
        self.btn_connect = ttk.Button(self.conn_frame, text="Connect", style="Connect.TButton")
        self.lbl_connection_status = ttk.Label(self.conn_frame, text="STATUS: DISCONNECTED", font=("Helvetica", 10, "bold"))

        self.file_frame = ttk.Frame(self)
        
        self.btn_select_file = ttk.Button(self.file_frame, text="Select File", style="Standard.TButton")
        self.lbl_selected_file = ttk.Label(self.file_frame, text="No file selected...")
        self.btn_lock = ttk.Button(self.file_frame, text="Lock", style="Lock.TButton")

        self.log_box = scrolledtext.ScrolledText(
            self, 
            state='disabled', 
            height=5, 
            width=50,
            bg=self.log_bg,           
            fg=self.text_color,
            insertbackground=self.text_color, 
            font=("Consolas", 9), 
            relief="flat",        
            highlightthickness=1, 
            highlightbackground=self.log_border
        )

        self.log_box.vbar.configure(
            bg=self.log_border,              
            troughcolor=self.bg_color,        
            activebackground=self.cyan_accent, 
            borderwidth=0,
            elementborderwidth=0
        )

    def setup_layout(self):
        self.conn_frame.pack(side=tk.TOP, fill=tk.X, padx=15, pady=(20, 10))
        self.btn_connect.pack(side=tk.LEFT, padx=(0, 15))
        self.lbl_connection_status.pack(side=tk.LEFT)

        self.file_frame.pack(side=tk.TOP, fill=tk.X, padx=15, pady=10)
        self.btn_select_file.pack(side=tk.LEFT, padx=(0, 15))
        self.lbl_selected_file.pack(side=tk.LEFT, expand=True, fill=tk.X) 
        self.btn_lock.pack(side=tk.RIGHT)

        self.log_box.pack(side=tk.BOTTOM, fill=tk.BOTH, expand=True, padx=15, pady=(10, 20))

    def bind_connect_button(self, callback):
        self.btn_connect.config(command=callback)

    def bind_select_file_button(self, callback):
        self.btn_select_file.config(command=callback)

    def bind_lock_button(self, callback):
        self.btn_lock.config(command=callback)

    def update_connection_status(self, text, is_connected=False):
        self.lbl_connection_status.config(text=f"STATUS: {text.upper()}")
        self.btn_connect.config(text="Disconnect" if is_connected else "Connect")

    def update_selected_file(self, filename):
        self.lbl_selected_file.config(text=filename)

    def update_action_button(self, new_text):
        self.btn_lock.config(text=new_text)

    def append_log(self, message):
        self.log_box.config(state='normal')
        self.log_box.insert(tk.END, message + "\n")
        self.log_box.see(tk.END)
        self.log_box.config(state='disabled')