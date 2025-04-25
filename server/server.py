from flask import Flask, request

app = Flask(__name__)

@app.route("/")
def hello_world():
    return "<p>Hello</p>"

@app.route('/sensor', methods=['POST'])
def handle_sensor():
    data = request.get_json()
    print(f"Received: {data}")
    return {'status': 'ok'}, 200


if __name__ == "__main__":
    app.run(host="127.0.0.1", port=5000)