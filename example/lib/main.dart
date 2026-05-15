import 'dart:async';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:layrz_wifi/layrz_wifi.dart';
import 'package:permission_handler/permission_handler.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return const MaterialApp(title: 'layrz_wifi example', home: WifiDemoPage());
  }
}

class WifiDemoPage extends StatefulWidget {
  const WifiDemoPage({super.key});

  @override
  State<WifiDemoPage> createState() => _WifiDemoPageState();
}

class _WifiDemoPageState extends State<WifiDemoPage> {
  final _wifi = LayrzWifi.instance;

  bool? _hasDiscovery;
  bool? _hasCurrentSsid;
  String? _currentSsid;
  final List<WifiNetwork> _networks = [];
  String? _status;
  bool _scanning = false;

  StreamSubscription<WifiNetwork>? _scanSub;
  StreamSubscription<WifiScanEvent>? _eventSub;

  @override
  void initState() {
    super.initState();
    _probe();
  }

  @override
  void dispose() {
    _scanSub?.cancel();
    _eventSub?.cancel();
    super.dispose();
  }

  Future<void> _probe() async {
    try {
      final disc = await _wifi.hasDiscovery();
      final ssid = await _wifi.hasCurrentSsid();
      setState(() {
        _hasDiscovery = disc;
        _hasCurrentSsid = ssid;
      });
      if (ssid) await _fetchCurrentSsid();
    } on PlatformException catch (e) {
      setState(() => _status = 'Probe error: ${e.message}');
    }
  }

  Future<void> _fetchCurrentSsid() async {
    try {
      final ssid = await _wifi.currentSsid();
      setState(() => _currentSsid = ssid ?? '(not connected)');
    } on PlatformException catch (e) {
      setState(() => _currentSsid = 'Error: ${e.message}');
    }
  }

  /// Requests the platform permission needed for Wi-Fi scanning.
  /// Returns true if the app may proceed with scanning.
  Future<bool> _requestPermission() async {
    // Linux, Windows and macOS: delegate entirely to the plugin's ensurePermissions()
    // (which calls CLLocationManager on Apple, notRequired on others).
    if (Platform.isLinux || Platform.isWindows || Platform.isMacOS) return true;

    if (Platform.isAndroid) {
      final statuses = await [
        Permission.locationWhenInUse,
        Permission.nearbyWifiDevices,
      ].request();
      final granted = statuses.values.any((s) => s.isGranted);
      if (!granted) setState(() => _status = 'Permission denied');
      return granted;
    }

    var status = await Permission.locationWhenInUse.status;
    if (status.isGranted) return true;
    if (status.isPermanentlyDenied) {
      setState(
        () => _status =
            'Permission permanently denied — open Settings to enable it.',
      );
      await openAppSettings();
      return false;
    }
    status = await Permission.locationWhenInUse.request();
    if (status.isGranted) return true;
    setState(() => _status = 'Permission denied: $status');
    return false;
  }

  Future<void> _startScan() async {
    final allowed = await _requestPermission();
    if (!allowed) return;

    final granted = await _wifi.requestPermissions();
    if (!granted) {
      final status = await _wifi.permissionStatus();
      setState(() => _status = 'Permission denied: $status');
      return;
    }

    setState(() {
      _scanning = true;
      _networks.clear();
      _status = 'Scanning…';
    });

    _scanSub = _wifi.scanResults.listen(
      (network) => setState(() => _networks.add(network)),
    );

    _eventSub = _wifi.scanEvents.listen((event) {
      if (event is WifiScanComplete) {
        setState(() {
          _scanning = false;
          _status = 'Found ${_networks.length} networks';
        });
        _scanSub?.cancel();
        _eventSub?.cancel();
      } else if (event is WifiScanError) {
        setState(() {
          _scanning = false;
          _status = 'Scan error: ${event.message}';
        });
        _scanSub?.cancel();
        _eventSub?.cancel();
      }
    });

    try {
      await _wifi.startScan();
    } on PlatformException catch (e) {
      setState(() {
        _status = 'Scan error: ${e.message}';
        _scanning = false;
      });
      _scanSub?.cancel();
    }
  }

  Future<void> _stopScan() async {
    await _wifi.stopScan();
    await _scanSub?.cancel();
    await _eventSub?.cancel();
    _scanSub = null;
    _eventSub = null;
    setState(() {
      _scanning = false;
      _status = 'Stopped — found ${_networks.length} networks';
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('layrz_wifi')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          _row('hasDiscovery', _hasDiscovery?.toString() ?? '…'),
          _row('hasCurrentSsid', _hasCurrentSsid?.toString() ?? '…'),
          _row('currentSsid', _currentSsid ?? '…'),
          const Divider(),
          Row(
            children: [
              Expanded(
                child: ElevatedButton(
                  onPressed: (_hasDiscovery == true && !_scanning)
                      ? _startScan
                      : null,
                  child: const Text('Start Scan'),
                ),
              ),
              const SizedBox(width: 8),
              Expanded(
                child: ElevatedButton(
                  onPressed: _scanning ? _stopScan : null,
                  child: const Text('Stop Scan'),
                ),
              ),
            ],
          ),
          if (_scanning) ...[
            const SizedBox(height: 8),
            const LinearProgressIndicator(),
          ],
          if (_status != null) ...[
            const SizedBox(height: 8),
            Text(_status!, style: const TextStyle(fontStyle: FontStyle.italic)),
          ],
          const SizedBox(height: 16),
          ..._networks.map(
            (n) => Card(
              child: ListTile(
                title: Text(n.ssid.isEmpty ? '(hidden)' : n.ssid),
                subtitle: Text(
                  '${n.bssid ?? ''} · ${n.security.name} · ${n.signalDbm ?? '?'} dBm',
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _row(String label, String value) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        children: [
          Text('$label: ', style: const TextStyle(fontWeight: FontWeight.bold)),
          Text(value),
        ],
      ),
    );
  }
}
