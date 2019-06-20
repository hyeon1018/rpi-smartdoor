#-*- coding:utf-8 -*-
import paho.mqtt.client as mqtt
import picamera
import io
import os
import json

device_name = "door"

device_topic = "device/{}/".format(device_name)

accpet_text = "비밀번호가 맞았습니다."

reject_text = "틀린 비밀번호가 입력되었습니다."

change_text = "비밀번호가 변경되었습니다."

alter_text = "문이 열려있습니다."

dev = os.open("/dev/doorlock_dev", os.O_RDWR)

client = mqtt.Client("Door")
client.will_set(device_topic + "connected", "False", 0, False)
client.connect("localhost", 1883, 0)
client.publish(device_topic + "connected", "True")

def publish_image():
	stream = io.BytesIO()
	with picamera.PiCamera() as camera:
		camera.start_preview()
		camera.capture(stream, format = 'jpeg')
	stream.seek(0)
	client.publish(device_topic + "image", stream.read())

while True :
	msg = os.read(dev, 10)
	if msg == "ACCEPT" :
		print("ACCEPT")
		client.publish(device_topic + "text", accpet_text)
		publish_image()
	elif msg == "REJECT" :
		print("REJECT")
		client.publish(device_topic + "text", reject_text)
		publish_image()
	elif msg == "CHANGE" :
		print("change")
		client.publish(device_topic + "text", change_text)
	elif msg == "ALTER" : 
		print("ALTER")
		client.publish(device_topic + "text", alter_text)
	else:
		print("error")
		client.publish(device_topic + "text", accpet_text)

