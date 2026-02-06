import serial
import csv
from datetime import datetime

# Konfiguracja portu (sprawdź w Arduino IDE: Narzędzia -> Port)
SERIAL_PORT = 'COM4'  # Windows: COM3, COM4 itp.
                       # Linux/Mac: /dev/ttyUSB0 lub /dev/ttyACM0
BAUD_RATE = 9600

# Nazwa pliku z timestampem
filename = f'puls_data_{datetime.now().strftime("%Y%m%d_%H%M%S")}.csv'

try:
    # Połączenie z Arduino
    arduino = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    print(f"Połączono z Arduino na porcie {SERIAL_PORT}")
    print(f"Zapisywanie do pliku: {filename}")
    
    # Otwórz plik CSV
    with open(filename, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        # Nagłówek
        writer.writerow(['Czas', 'Tetno_BPM'])
        
        print("Naciśnij Ctrl+C aby zakończyć...\n")
        
        while True:
            if arduino.in_waiting > 0:
                # Odczytaj linię z Arduino
                line = arduino.readline().decode('utf-8').strip()
                
                if line:  # Jeśli linia nie jest pusta
                    try:
                        bpm = float(line)
                        timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
                        
                        # Zapisz do CSV
                        writer.writerow([timestamp, bpm])
                        csvfile.flush()  # Natychmiastowy zapis
                        
                        # Wyświetl na ekranie
                        print(f"{timestamp} | Tętno: {bpm:.1f} BPM")
                    except ValueError:
                        print(f"Błędne dane: {line}")

except KeyboardInterrupt:
    print("\n\nZakończono zapisywanie.")
    
except serial.SerialException as e:
    print(f"Błąd portu szeregowego: {e}")
    print("Sprawdź czy:")
    print("1. Arduino jest podłączone")
    print("2. Port COM jest poprawny")
    print("3. Zamknąłeś Serial Monitor w Arduino IDE")

finally:
    if 'arduino' in locals():
        arduino.close()
        print("Port zamknięty.")