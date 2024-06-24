# Gesture-Based-Voice-Recorder

This project utilizes a device called the M5Core2 and its built-in microphone and accelerometer to create a gesture-based voice recorder.​ This involved two devices connected via Bluetooth low-energy. The server was responsible for the gesture interpretation while the client device was connected to a VCNL4040 proximity sensor that controlled the volume of the recording playback. The activation gestures are as follows: 

Shaking the device up/down initiates the voice recorder​

Shaking the device left/right initiates the recording playback​

Alternatively, the user can use the touchscreen buttons to initiate these features​

Moving closer/farther away from the client device changes the playback volume.

The server device also involved API calls to Google Cloud Functions that stored a log of what the device did in could storage. 
