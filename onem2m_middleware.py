from flask import Flask, request, jsonify
from datetime import datetime

app = Flask(__name__)

onem2m_data = {
    "CSEBase": {
        "SmartDoorSecuritySystem_AE": {
            "AccessLog": [],
            "DoorStatus": [],
            "SecurityAlert": []
        }
    }
}

@app.route("/")
def home():
    return jsonify({
        "message": "Simulated oneM2M Middleware is running",
        "AE": "SmartDoorSecuritySystem_AE"
    })

@app.route("/onem2m/access-log", methods=["POST"])
def access_log():
    data = request.json

    content_instance = {
        "rfid_uid": data.get("rfid_uid"),
        "rfid_status": data.get("rfid_status"),
        "pin_status": data.get("pin_status"),
        "access_status": data.get("access_status"),
        "failed_attempts": data.get("failed_attempts"),
        "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    }

    onem2m_data["CSEBase"]["SmartDoorSecuritySystem_AE"]["AccessLog"].append(content_instance)

    return jsonify({
        "message": "AccessLog contentInstance created",
        "contentInstance": content_instance
    }), 201

@app.route("/onem2m/door-status", methods=["POST"])
def door_status():
    data = request.json

    content_instance = {
        "door_status": data.get("door_status"),
        "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    }

    onem2m_data["CSEBase"]["SmartDoorSecuritySystem_AE"]["DoorStatus"].append(content_instance)

    return jsonify({
        "message": "DoorStatus contentInstance created",
        "contentInstance": content_instance
    }), 201

@app.route("/onem2m/security-alert", methods=["POST"])
def security_alert():
    data = request.json

    content_instance = {
        "alert_status": data.get("alert_status"),
        "reason": data.get("reason"),
        "failed_attempts": data.get("failed_attempts"),
        "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    }

    onem2m_data["CSEBase"]["SmartDoorSecuritySystem_AE"]["SecurityAlert"].append(content_instance)

    return jsonify({
        "message": "SecurityAlert contentInstance created",
        "contentInstance": content_instance
    }), 201

@app.route("/onem2m/data", methods=["GET"])
def get_all_data():
    return jsonify(onem2m_data)

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)