@echo off
echo Building Test Runner...
if not exist "test\build" mkdir "test\build"
g++ -o test\build\test_runner.exe test/testbenches/main.cpp test/mocks/mocks.cpp src/PS2_controller.cpp src/phoenix_driver_ssc32.cpp src/Leg.cpp src/Hexapod.cpp src/BotLight.cpp -I test/mocks -I src -D ARDUINO=100 -D __AVR__ -D USEPS2 -D __BOTBOARD_ARDUINOPROMINI__ -D USE_SSC32 -D MOCK_HARDWARE
if %ERRORLEVEL% NEQ 0 (
    echo Build Failed!
    exit /b %ERRORLEVEL%
)
echo Build Successful. Running Tests...
.\test\build\test_runner.exe
echo Done.
