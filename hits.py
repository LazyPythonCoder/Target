import re
import time
import clr

clr.AddReference("System.Windows.Forms")
from System.Windows.Forms import Application, Form, TextBox, DockStyle, ScrollBars

# Настройка окна
form = Form()
form.Text = "Монитор попаданий (все роверы)"
form.Width = 450
form.Height = 360
form.TopMost = True

box = TextBox()
box.Multiline = True
box.ReadOnly = True
box.ScrollBars = ScrollBars.Vertical
box.Dock = DockStyle.Fill
box.Text = "Ожидание данных от роверов..."
form.Controls.Add(box)

# Словарь для хранения последних сообщений каждого ровера: {sysid: "текст"}
last_msgs = {}
# Словарь с актуальными данными для отображения: {sysid: "строка вывода"}
rover_data = {}

ZONES = {
    "1": "Голова",
    "2": "Туловище"
}


def update_ui():
    updated = False

    # Перебираем все подключенные MAVLink-системы в Mission Planner
    for mav in MAV.MAVlist:
        sysid = mav.sysid
        if sysid == 0:
            continue

        current_msg = mav.cs.message
        if current_msg and current_msg != last_msgs.get(sysid):
            last_msgs[sysid] = current_msg

            match = re.search(r"(\d+)-(\d+)-(\d+)", current_msg)
            if match:
                m, n, zone_code = match.groups()
                zone_name = ZONES.get(zone_code, "Неизвестно ({0})".format(zone_code))

                rover_data[sysid] = (
                    "=== РОВЕР (ID: {0}) ===\r\n"
                    "Мишень №: {1} | Попаданий (N): {2} | Зона: {3}\r\n"
                ).format(sysid, m, n, zone_name)
                updated = True

    if updated and rover_data:
        # Сортируем вывод по ID ровера и обновляем окно
        lines = [rover_data[sid] for sid in sorted(rover_data.keys())]
        box.Text = "\r\n".join(lines)


form.Show()
while form.Visible:
    update_ui()
    Application.DoEvents()
    time.sleep(0.1)
