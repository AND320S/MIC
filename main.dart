import 'dart:convert';
import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:flutter_tts/flutter_tts.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:flutter_bluetooth_serial/flutter_bluetooth_serial.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Hand Signal to Speech',
      theme: ThemeData(primarySwatch: Colors.blue),
      home: const BluetoothPage(),
    );
  }
}

class BluetoothPage extends StatefulWidget {
  const BluetoothPage({super.key});

  @override
  State<BluetoothPage> createState() => _BluetoothPageState();
}

class _BluetoothPageState extends State<BluetoothPage> {
  final FlutterTts flutterTts = FlutterTts();
  BluetoothConnection? _connection;
  bool isConnecting = true;
  bool isConnected = false;
  String _buffer = "";
  String _lastPhrase = ""; // <-- NEW: To store the last received phrase

  @override
  void initState() {
    super.initState();
    _initPermissions();
  }

  Future<void> _initPermissions() async {
    await [
      Permission.bluetooth,
      Permission.bluetoothScan,
      Permission.bluetoothConnect,
      Permission.location
    ].request();
  }

  Future<void> _connectToESP32() async {
    try {
      final devices = await FlutterBluetoothSerial.instance.getBondedDevices();

      BluetoothDevice? device = devices.firstWhere(
        (d) => d.name == "ESP32",
        orElse: () => throw Exception("ESP32 not found in paired devices"),
      );

      BluetoothConnection connection = await BluetoothConnection.toAddress(device.address);
      print('Connected to the device');

      setState(() {
        _connection = connection;
        isConnected = true;
        isConnecting = false;
      });

      connection.input!.listen((Uint8List data) {
        _buffer += ascii.decode(data);

        while (_buffer.contains('\n')) {
          int index = _buffer.indexOf('\n');
          String message = _buffer.substring(0, index).trim();
          _buffer = _buffer.substring(index + 1);
          print('Received: $message');
          _handleSignal(message);
        }
      }).onDone(() {
        print('Disconnected by remote request');
        setState(() {
          isConnected = false;
        });
      });
    } catch (e) {
      print('Error connecting: $e');
    }
  }
void _handleSignal(String data) {
  String phrase = data.trim(); // Directly use the received data

  setState(() {
    _lastPhrase = phrase;
  });

  flutterTts.speak(phrase);
}


  @override
  void dispose() {
    _connection?.dispose();
    super.dispose();
  }

@override
Widget build(BuildContext context) {
  return Scaffold(
    backgroundColor: const Color(0xFFEFF3F6), // soft background
    appBar: AppBar(
      title: const Text("Hand Signal to Speech"),
      backgroundColor: const Color.fromARGB(255, 255, 255, 255),
      elevation: 4,
    ),
    body: Center(
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 20.0, vertical: 10),
        child: isConnected
            ? Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  const Text(
                    "MIC Project",
                    style: TextStyle(
                      fontSize: 26,
                      fontWeight: FontWeight.bold,
                      color: Colors.black87,
                    ),
                  ),
                  const SizedBox(height: 6),
                  const Text(
                    "Electric Elephants",
                    style: TextStyle(
                      fontSize: 30,
                      fontWeight: FontWeight.bold,
                      color: Colors.blueAccent,
                    ),
                  ),
                  const SizedBox(height: 6),
                  const Text(
                    "Smart Glove Project",
                    style: TextStyle(
                      fontSize: 20,
                      fontStyle: FontStyle.italic,
                      color: Colors.black54,
                    ),
                  ),
                  const SizedBox(height: 40),
                  Stack(
                    alignment: Alignment.center,
                    children: [
                      AnimatedContainer(
                        duration: const Duration(milliseconds: 800),
                        curve: Curves.easeOut,
                        width: _lastPhrase.isNotEmpty ? 280 : 220,
                        height: _lastPhrase.isNotEmpty ? 280 : 220,
                        decoration: BoxDecoration(
                          shape: BoxShape.circle,
                          gradient: RadialGradient(
                            colors: [
                              Colors.blueAccent.withOpacity(0.3),
                              Colors.blueAccent.withOpacity(0.05),
                            ],
                          ),
                        ),
                      ),
                      AnimatedContainer(
                        duration: const Duration(milliseconds: 800),
                        curve: Curves.easeOut,
                        width: _lastPhrase.isNotEmpty ? 200 : 150,
                        height: _lastPhrase.isNotEmpty ? 200 : 150,
                        decoration: BoxDecoration(
                          shape: BoxShape.circle,
                          color: Colors.blueAccent.withOpacity(0.2),
                        ),
                      ),
                      AnimatedSwitcher(
                        duration: const Duration(milliseconds: 600),
                        transitionBuilder: (Widget child, Animation<double> animation) {
                          return ScaleTransition(
                            scale: CurvedAnimation(
                              parent: animation,
                              curve: Curves.easeOutBack,
                            ),
                            child: FadeTransition(
                              opacity: animation,
                              child: child,
                            ),
                          );
                        },
                        child: Text(
                          _lastPhrase.isNotEmpty ? _lastPhrase : "Waiting...",
                          key: ValueKey<String>(_lastPhrase),
                          style: TextStyle(
                            fontSize: _lastPhrase.isNotEmpty ? 40 : 30,
                            fontWeight: FontWeight.bold,
                            color: Colors.blueAccent,
                          ),
                          textAlign: TextAlign.center,
                        ),
                      ),
                    ],
                  ),
                ],
              )
            : Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  const Text(
                    "Smart Glove Connection",
                    style: TextStyle(
                      fontSize: 28,
                      fontWeight: FontWeight.bold,
                      color: Colors.black87,
                    ),
                  ),
                  const SizedBox(height: 40),
                  ElevatedButton(
                    onPressed: _connectToESP32,
                    style: ElevatedButton.styleFrom(
                      backgroundColor: Colors.blueAccent,
                      padding: const EdgeInsets.symmetric(horizontal: 30, vertical: 18),
                      textStyle: const TextStyle(fontSize: 24),
                      shape: RoundedRectangleBorder(
                        borderRadius: BorderRadius.circular(12),
                      ),
                      elevation: 6,
                    ),
                    child: const Text("Connect to ESP32"),
                  ),
                ],
              ),
      ),
    ),
  );
}
}