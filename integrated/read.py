import paho.mqtt.client as mqtt
import picamera
import io
import os
import json

device_name = "door"

client = mqtt.Client("Door")
client.will_set("device/{}/connected".format(device_name), "False", 0, False)
client.connect("localhost", 1883, 60)
client.publish("device/{}/connected".format(device_name), "True")

def publish_image():
	stream = io.BytesIO()
	with picamera.PiCamera() as camera:
		camera.start_preview()
		camera.capture(stream, format = 'jpeg')	
	stream.seek(0)
	client.publish("device/{}/image".format(device_name), stream.read())


dev = os.open("/dev/doorlock_dev", os.O_RDWR)


while True :
	msg = os.read(dev, 10)
	if msg == "ACCEPT" :
		print("ACCEPT")
		publish_image()
	elif msg == "REJECT" :
		print("REJECT")
	elif msg == "CHANGE" :
		print("change")
	elif msg == "ALTER" : 
		print("ALTER")
	else:
		print("error")

