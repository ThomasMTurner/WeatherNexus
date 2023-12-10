import model
import send

def main():
    future_temperatures = model.get_predictions()
    send.send_predictions(future_temperatures)


if __name__ == "__main__":
    main()