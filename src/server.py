from flask import Flask, render_template, request
from flask_socketio import SocketIO, emit

app = Flask(__name__)
socketio = SocketIO(app)

@app.route("/", methods=["GET"])
def update():
    temp = request.args.get("temp")
    tds = request.args.get("tds")
    print(f"Received: temp={temp}, tds={tds}")
    socketio.emit('new_data', {'temp': temp, 'tds': tds})
    return "Data updated."

@app.route("/graph")
def graph():
    return render_template("index.html")

if __name__ == '__main__':
    socketio.run(app, host='0.0.0.0')
