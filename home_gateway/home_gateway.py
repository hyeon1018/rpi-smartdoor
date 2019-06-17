import paho.mqtt.client as mqtt
import telegram
import json
import io

# telegram token and chat id.
my_token = ''
my_chat_id = 0
bot = telegram.Bot(token = my_token)

def on_log(client, userdata, level, buf):
	print("log : " + buf)

def on_connect(client, userdata, flags, rc):
	client.subscribe("device/#")

def on_message(client, userdata, msg):
	print("message get : " + str(msg.topic))
	topic = str(msg.topic).split('/')
	if topic[2] == "text":
		text = msg.payload.decode('utf-8')
		bot.sendMessage(chat_id=my_chat_id, text = "[{}]\n{}".format(topic[1], text))
	elif topic[2] == "image":
		bio = io.BytesIO(msg.payload)
		bio.seek(0)
		bot.send_photo(chat_id=my_chat_id, photo = bio)
	elif topic[2] == "connected":
		state = msg.payload.decode('utf-8')
		if state == "True":
			bot.sendMessage(chat_id=my_chat_id, text = "New Device Connected : {}".format(topic[1]))
		elif state == "False":
			bot.sendMessage(chat_id=my_chat_id, text = "Device Disconnected : {}".format(topic[1]))
	else:
		print(msg.payload)

client = mqtt.Client()
client.on_log = on_log
client.on_connect = on_connect
client.on_message = on_message

client.connect("127.0.0.1")

client.loop_forever()
