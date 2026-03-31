import os
from datetime import datetime
from sklearn.linear_model import LinearRegression
import pandas as pd
from flask import Flask, request, render_template, jsonify
from flask import Flask
import csv


app = Flask(__name__)


API_TOKEN = "vlad_meteo_pass_777"

# Файл базы данных
DATA_FILE = 'meteo_data.csv'

# --- ЧАСТЬ 1: РАБОТА С ДАННЫМИ ---


def save_to_csv(data):
    file_path = DATA_FILE
    header = ['timestamp', 'temp', 'hum', 'press']

    file_exist = os.path.isfile(file_path)

    current_time = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

    row = [
        current_time,
        data.get('temp'),
        data.get('hum'),
        data.get('press')
    ]

    with open(file_path, mode='a', newline='', encoding='utf-8') as f:
        writer = csv.writer(f, delimiter=',')

        if not file_exist:
            writer.writerow(header)

        writer.writerow(row)


def get_forecast(temp_now, press_now):
    # ГУГЛИТЬ: "scikit-learn linear regression predict"
    # Это самая сложная часть. Сначала нужно накопить данные в CSV,
    # потом обучить модель и вызвать .predict()
    # Для начала можно вернуть "заглушку":
    return {"temp_tomorrow": temp_now + 1, "status": "Stable"}

# --- ЧАСТЬ 2: API ДЛЯ ESP32 ---


@app.route('/update', methods=['POST'])
def update_data():
    data = request.get_json()
    # Проверяем, совпадает ли токен из пакета с нашим секретом
    if data.get('token') != API_TOKEN:
        return jsonify({"status": "error", "message": "Access denied"}), 403

    # Если прошли проверку — сохраняем в CSV
    save_to_csv(data)
    return jsonify({"status": "received"}), 200

# --- ЧАСТЬ 3: ВЕБ-ИНТЕРФЕЙС ---


@app.route('/')
def index():
    # ГУГЛИТЬ: "pandas read last n rows from csv"
    # Тебе нужно прочитать последние данные, чтобы отобразить их на сайте

    # ГУГЛИТЬ: "flask render_template pass variables"
    return render_template('index.html', temp=25.0, forecast="Sunny")


if __name__ == '__main__':
    # host='0.0.0.0' — чтобы ESP32 видела ноут в сети
    # port=8742 — наш "секретный" порт
    app.run(host='0.0.0.0', port=8742, debug=True)


# «Для защиты от подмены данных и несанкционированного доступа к API я
# реализовал проверку по статическому токену (API Key). Данные принимаются
# сервером только при наличии валидного ключа в JSON-пакете»
