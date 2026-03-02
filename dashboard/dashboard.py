"""
dashboard.py - Live sensor data dashboard
==========================================
Works in two modes:

  MODE 1 - CSV file (Phase 2, PC simulation):
    python dashboard/dashboard.py

  MODE 2 - Arduino serial (Phase 3, real hardware):
    python dashboard/dashboard.py --serial COM3
    (replace COM3 with your actual Arduino port)

To find your Arduino port:
  Windows: Device Manager -> Ports (COM & LPT) -> Arduino Uno (COMx)
  Or: Arduino IDE -> Tools -> Port

Requirements:
    pip install matplotlib pandas pyserial
"""

import os
import sys
import argparse
import threading
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.lines import Line2D

# =============================================================================
# CONFIGURATION
# =============================================================================

CSV_PATH    = "data/sensor_log.csv"   # Phase 2 CSV from C program
SERIAL_CSV  = "data/serial_log.csv"   # Phase 3 CSV written from serial
BAUD_RATE   = 9600                     # Must match Serial.begin() in Arduino
REFRESH_MS  = 5000                     # Dashboard refresh interval

THRESHOLDS = {
    "Temperature C": {
        "Warn High (70C)":      70.0,
        "Critical High (85C)":  85.0,
    },
    "Humidity (%)": {
        "Warn Low (20%)":       20.0,
        "Warn High (80%)":      80.0,
    },
    "Vibration (g)": {
        "Warn High (0.5g)":     0.5,
        "Critical High (1.0g)": 1.0,
    },
    "Current Draw (A)": {
        "Warn High (8A)":       8.0,
        "Critical High (10A)":  10.0,
    },
}

COLOURS = {
    "NONE":     "#2196F3",
    "WARNING":  "#FF9800",
    "CRITICAL": "#F44336",
}

# =============================================================================
# SERIAL READER THREAD
# Runs in background, reads lines from Arduino, appends to CSV
# =============================================================================

serial_active = False

def serial_reader(port, baud, output_csv):
    """
    Background thread that reads CSV lines from Arduino over USB
    and appends them to output_csv for the dashboard to plot.
    Lines starting with # are Arduino comments - printed, not saved.
    """
    global serial_active
    try:
        import serial
    except ImportError:
        print("\nERROR: pyserial not installed. Run: pip install pyserial")
        return

    print(f"[SERIAL] Connecting to {port} at {baud} baud...")
    try:
        ser = serial.Serial(port, baud, timeout=2)
    except Exception as e:
        print(f"[SERIAL] ERROR: Could not open {port}: {e}")
        print("[SERIAL] Check Device Manager -> Ports for your Arduino port")
        return

    print(f"[SERIAL] Connected to {port}")
    serial_active = True

    os.makedirs(os.path.dirname(output_csv), exist_ok=True)
    header_needed = (not os.path.exists(output_csv) or
                     os.path.getsize(output_csv) == 0)

    with open(output_csv, "a", buffering=1) as f:
        if header_needed:
            f.write("timestamp,sensor_id,sensor_name,value,alert_level\n")

        while True:
            try:
                raw  = ser.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="replace").strip()
                if not line:
                    continue
                if line.startswith("#"):
                    print(f"[ARDUINO] {line[1:].strip()}")
                    continue
                if line.startswith("timestamp"):
                    continue   # skip header row Arduino sends on boot
                parts = line.split(",")
                if len(parts) != 5:
                    continue   # malformed line
                f.write(line + "\n")
                f.flush()
                print(f"[SERIAL] {line}")
            except Exception as e:
                print(f"[SERIAL] Read error: {e}")
                break

    ser.close()
    serial_active = False

# =============================================================================
# CSV LOADER
# =============================================================================

def load_csv(path):
    if not os.path.exists(path):
        return pd.DataFrame()
    try:
        df = pd.read_csv(path)
        required = {"timestamp", "sensor_id", "sensor_name",
                    "value", "alert_level"}
        if not required.issubset(df.columns):
            return pd.DataFrame()
        return df
    except Exception:
        return pd.DataFrame()

# =============================================================================
# LEGEND
# =============================================================================

legend_elements = [
    Line2D([0], [0], color=COLOURS["NONE"],     linewidth=2, label="Normal"),
    Line2D([0], [0], color=COLOURS["WARNING"],  linewidth=2, label="Warning"),
    Line2D([0], [0], color=COLOURS["CRITICAL"], linewidth=2, label="Critical"),
]

# =============================================================================
# UPDATE FUNCTION - called every REFRESH_MS milliseconds
# =============================================================================

def make_updater(fig, axes, csv_path):
    def update(frame):
        df = load_csv(csv_path)

        if df.empty:
            for ax in axes:
                ax.clear()
                ax.set_title("Waiting for data...", color="gray")
                ax.text(0.5, 0.5,
                    "No data yet.\n"
                    "Run .\\build\\sensor_logger.exe  OR  connect Arduino.",
                    transform=ax.transAxes, ha="center", va="center",
                    color="gray", fontsize=10)
            return

        sensor_names = list(df["sensor_name"].unique())
        last_alert   = "NONE"
        total_rows   = len(df)

        for idx, ax in enumerate(axes):
            ax.clear()
            if idx >= len(sensor_names):
                ax.set_visible(False)
                continue

            name      = sensor_names[idx]
            sensor_df = df[df["sensor_name"] == name].copy()
            if sensor_df.empty:
                ax.set_title(f"{name} - No readings yet", color="gray")
                continue

            timestamps = sensor_df["timestamp"].values
            values     = sensor_df["value"].values
            alerts     = sensor_df["alert_level"].values

            for i in range(len(timestamps)):
                colour = COLOURS.get(str(alerts[i]), COLOURS["NONE"])
                ax.plot(timestamps[i], values[i], "o",
                        color=colour, markersize=5, zorder=3)
                if i > 0:
                    ax.plot([timestamps[i-1], timestamps[i]],
                            [values[i-1],     values[i]],
                            color=colour, linewidth=1.5, zorder=2)
                if str(alerts[i]) != "NONE":
                    last_alert = f"{name}: {alerts[i]} ({values[i]:.2f})"

            if name in THRESHOLDS:
                styles  = ["--", ":",  "-.", "-"]
                colours = [COLOURS["WARNING"], COLOURS["CRITICAL"],
                           COLOURS["WARNING"], COLOURS["CRITICAL"]]
                for j, (label, threshold) in enumerate(
                        THRESHOLDS[name].items()):
                    ax.axhline(y=threshold,
                               color=colours[j % len(colours)],
                               linestyle=styles[j % len(styles)],
                               linewidth=1.2, alpha=0.7, label=label)

            latest_val   = values[-1]
            latest_alert = str(alerts[-1])
            alert_colour = COLOURS.get(latest_alert, COLOURS["NONE"])

            ax.set_title(
                f"{name}  |  Latest: {latest_val:.2f}"
                f"  |  Status: {latest_alert}",
                color=alert_colour, fontweight="bold", fontsize=10
            )
            ax.set_xlabel("Timestamp (ms)", fontsize=8)
            ax.set_ylabel("Value", fontsize=8)
            ax.grid(True, alpha=0.3)
            ax.legend(loc="upper left", fontsize=7)

        fig.legend(handles=legend_elements, loc="lower center",
                   ncol=3, fontsize=9, framealpha=0.9,
                   bbox_to_anchor=(0.5, 0.01))

        alert_display = (f"Last alert: {last_alert}"
                         if last_alert != "NONE" else "No alerts")
        source = "Arduino (live)" if serial_active else "CSV file"
        fig.suptitle(
            f"Predictive Maintenance Monitor  |  {total_rows} readings"
            f"  |  {alert_display}  |  Source: {source}",
            fontsize=10, fontweight="bold"
        )

    return update

# =============================================================================
# MAIN
# =============================================================================

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Sensor Data Dashboard")
    parser.add_argument("--serial", metavar="PORT",
                        help="Arduino port e.g. --serial COM3")
    args = parser.parse_args()

    try:
        import matplotlib
        import pandas
    except ImportError as e:
        print(f"\nERROR: Missing dependency: {e}")
        print("Run: pip install matplotlib pandas pyserial")
        sys.exit(1)

    if args.serial:
        csv_path = SERIAL_CSV
        os.makedirs("data", exist_ok=True)
        t = threading.Thread(
            target=serial_reader,
            args=(args.serial, BAUD_RATE, SERIAL_CSV),
            daemon=True
        )
        t.start()
        print(f"[DASHBOARD] Serial mode on {args.serial}")
    else:
        csv_path = CSV_PATH
        print(f"[DASHBOARD] CSV mode  ({csv_path})")
        print("[DASHBOARD] For Arduino: python dashboard/dashboard.py --serial COM3")

    print(f"[DASHBOARD] Refreshing every {REFRESH_MS // 1000}s - close window to stop")

    fig, axes = plt.subplots(3, 1, figsize=(12, 9))
    fig.canvas.manager.set_window_title("Predictive Maintenance Monitor")
    plt.subplots_adjust(hspace=0.55, top=0.92)

    update = make_updater(fig, axes, csv_path)
    ani    = animation.FuncAnimation(fig, update,
                                     interval=REFRESH_MS,
                                     cache_frame_data=False)
    update(0)
    plt.tight_layout(rect=[0, 0.05, 1, 0.95])
    plt.show()
