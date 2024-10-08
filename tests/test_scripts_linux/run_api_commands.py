import zmq, time, json

HDTN_SOCKET_ADDR="tcp://127.0.0.1:10305"

def send(reqdata):
    context = zmq.Context()
    socket = zmq.Socket(context, zmq.REQ)
    socket.connect(HDTN_SOCKET_ADDR)
    start = time.time()
    socket.send_json(reqdata)
    resp = socket.recv()
    resp = json.dumps(json.loads(resp), indent=2)
    print()
    print()
    print("Result:")
    print(f'{resp}')
    print(f'in {(time.time() - start):.2f} seconds')
    print()


def get_expiring():
    reqdata = {
        "apiCall": "get_expiring_storage",
        "priority": 1,
        "thresholdSecondsFromNow": 600
    }
    send(reqdata)


def get_storage():
    reqdata = {
        "apiCall": "get_storage",
    }
    send(reqdata)


def get_inducts():
    reqdata = {
        "apiCall": "get_inducts",
    }
    send(reqdata)


def get_outducts():
    reqdata = {
        "apiCall": "get_outducts",
    }
    send(reqdata)


def get_outduct_capabilities():
    reqdata = {
        "apiCall": "get_outduct_capabilities",
    }
    send(reqdata)


def get_bpsec():
    reqdata = {
        "apiCall": "get_bpsec_config",
    }
    send(reqdata)


def set_max_send_rate(bps, outduct):
    reqdata = {
        "apiCall": "set_max_send_rate",
        "rateBitsPerSec": bps,
        "outduct": outduct,
    }
    send(reqdata)


def upload_contact_plan(plan):
    reqdata = {
        "apiCall": "upload_contact_plan",
        "contactPlanJson": json.dumps(plan)
    }
    send(reqdata)


def ping(version, node, service):
    reqdata = {
        "apiCall": "ping",
        "bpVersion": version,
        "nodeId": node,
        "serviceId": service
    }
    send(reqdata)

def get_current_hdtn_config():
    reqdata = {
        "apiCall": "get_hdtn_config",
    }
    send(reqdata)

def get_current_hdtn_version():
    reqdata = {
        "apiCall": "get_hdtn_version",
    }
    send(reqdata)

def take_link_down(outductArrayIndex):
    req = {
        "apiCall": "set_link_down",
        "outductIndex": outductArrayIndex
    }
    send(req)

def bring_link_up(outductArrayIndex):
    req = {
        "apiCall": "set_link_up",
        "outductIndex": outductArrayIndex
    }
    send(req)

while True:
    print("1: Get Storage")
    print("2: Get Expiring Storage")
    print("3: Get Outducts")
    print("4: Get Inducts")
    print("5: Ping")
    print("6: Get BPSec Config")
    print("7: Set Max Send Rate")
    print("8: Upload Contact Plan")
    print("9: Get Current HDTN Config")
    print("10: Get Current HDTN Version")
    print("11: Take link down")
    print("12: Bring link up")
    option = input("API command: ")

    if option == "1":
        get_storage()
    elif option == "2":
        get_expiring()
    elif option == "3":
        get_outducts()
    elif option == "4":
        get_inducts()
    elif option == "5":
        version = int(input("BP Version: "))
        node = int(input("Node: "))
        service = int(input("Service #: "))
        ping(version, node, service)
    elif option == "6":
        get_bpsec()
    elif option == "7":
        bps = int(input("Bits Per Second (bps): "))
        outduct = int(input("Outduct: "))
        set_max_send_rate(bps, outduct)
    elif option == "8":
        plan = input("Contact Plan File: ")
        with open(plan, "r") as text_file:
            contactPlan = json.load(text_file)
            upload_contact_plan(contactPlan)
    elif option == "9":
        get_current_hdtn_config()
    elif option == "10":
        get_current_hdtn_version()
    elif option == "11":
        outductArrayIndex = int(input("Index of link in outduct: "))
        take_link_down(outductArrayIndex)
    elif option == "12":
        outductArrayIndex = int(input("Index of link in outduct: "))
        bring_link_up(outductArrayIndex)
    else:
        print("Invalid option")
