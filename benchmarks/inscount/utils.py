import subprocess

def exec(command, args):
    result = subprocess.run([command] + args, capture_output=True, text=True)
    result