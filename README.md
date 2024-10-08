This project is about controlling a desk. following functionality is added:

oled screen sh1106
mechanical switches, 1 row of 5, a "matrix" of 2x3 switches. Some are going to be hardcoded, the row is going to function as a macropad.
sd cardreader, nvme drive, 2 x usb has been added externally by using a pcb.
Enclosure has been designed and 3d printed, to fit under a AudioEngine B2 speaker, used as a center speaker in the setup, however the STL files are made to fit desktop speakers i just glued it together. 

a 4 channel relay to control usb switches, such that all usb peripheals can be switched to another computer. also in the relay, the desks motor control

2 x 120 mm fans are connected to a mosfet, and can be controlled by adjusting a potentiometer, so they can blow, these are also connected to a razer argb controller for synapse sync. 

Rotary encoders are used to control the hue play bars behind my monitor. turn to adjust brightness, push for on/off
Rotary encoder is also used as a hid, and can mute, adjust volume. 

Oled screen will also show wifi status, time of day. pc1 or pc2, desk raising / lowering and distance while its being set. 

a single mechanical keyboard switch has been wired to the desktop, and can use the same functionality as the physical power button can. 

work to be done:

ota update and flashing. 

wiring RGB diodes into the keyboard switches, and giving visual feed back on key presses.

macropad functionality.

making a cli application to program the macropad, enter wifi details, set standing / sitting height. 

ble hid device for macropad, and volume control

pomodoro timer on the screen hard coded to a button. 

Hue scenes switching

distance sensor for automatic desk adjustment. 

