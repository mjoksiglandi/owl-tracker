# extra_flags.py
Import("env")

# Solo para C++ (g++)
env.Append(CXXFLAGS=["-fno-rtti", "-fno-exceptions"])

# Si quieres también forzar C++17 aquí (opcional, ya está en build_flags):
# env.Append(CXXFLAGS=["-std=gnu++17"])
