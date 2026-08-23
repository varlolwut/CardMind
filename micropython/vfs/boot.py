import esp32
import machine
import sys
import time

try:
    import cardmind_supervisor

    cardmind_supervisor.start()
except Exception as error:
    sys.print_exception(error)
    message = str(error)
    if len(message) > 190:
        message = message[:190]
    namespace = esp32.NVS("cardmind_py")
    namespace.set_blob("mode_error", message.encode("utf-8") + b"\x00")
    namespace.commit()
    matches = esp32.Partition.find(esp32.Partition.TYPE_APP, label="cardmind")
    if len(matches) != 1:
        raise RuntimeError("CardMind recovery partition is missing or ambiguous")
    print("PYTHON_MODE result=failed error={}".format(message))
    time.sleep_ms(1000)
    matches[0].set_boot()
    machine.reset()
