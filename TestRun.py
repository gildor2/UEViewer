import subprocess
import os

# Paths
umodel_path = "C:/Users/lauri/Documents/Github/UEViewer/umodel.exe"
game_path = "C:/Program Files/Epic Games/rocketleague"
package_path = os.path.join(game_path, "TAGame/CookedPCConsole/Stadium_P.upk")
output_path = "C:/Users/lauri/Desktop/RocketLeague/Automation"

# Command
command = [
    umodel_path,
    "-export",
    "-game=rocketleague",
    f"-path={game_path}",
    f"-out={output_path}",
    package_path
]

# Run UModel
try:
    subprocess.run(command, check=True)
    print("UModel export completed successfully.")
except subprocess.CalledProcessError as e:
    print(f"UModel failed with error code {e.returncode}")
except FileNotFoundError:
    print("UModel executable not found. Check the path.")