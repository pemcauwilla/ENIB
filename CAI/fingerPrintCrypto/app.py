# Import your modules
from view.main_view import MainView
from controller.main_controller import MainController
from model.bluetooth_model import BluetoothModel
import tkinter as tk

if __name__ == "__main__":
    root = tk.Tk()
    
    view = MainView(root)
    view.pack(fill=tk.BOTH, expand=True)
    root.configure(bg=view.bg_color)
    
    model = BluetoothModel()
    
    controller = MainController(root, view, model)
    
    root.mainloop()