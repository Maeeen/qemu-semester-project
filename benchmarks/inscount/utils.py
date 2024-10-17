import subprocess

def exec_cmd(command, args):
    result = subprocess.run([command] + args, capture_output=True, text=True)
    return result