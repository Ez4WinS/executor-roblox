-- Smoke test for ExecutorLab local sandbox (no Roblox process required).
print("executorlab-local-smoke")
print("value:", Host.multiply(6, 7))
print("mock-player:", game.Players.LocalPlayer.Name)
return Host.greet("Entwickler")
