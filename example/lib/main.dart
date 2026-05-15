import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:layrz_wifi/layrz_wifi.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return const MaterialApp(
      title: 'layrz_wifi example',
      home: WifiDemoPage(),
    );
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
  List<WifiNetwork> _networks = [];
  String? _status;
  bool _scanning = false;

  @override
  void initState() {
    super.initState();
    _probe();
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

  Future<void> _scan() async {
    setState(() {
      _scanning = true;
      _status = null;
    });
    try {
      final perm = await _wifi.ensurePermissions();
      if (perm != WifiPermissionStatus.granted &&
          perm != WifiPermissionStatus.notRequired) {
        setState(() {
          _status = 'Permission not granted: $perm';
          _scanning = false;
        });
        return;
      }
      final results = await _wifi.scan();
      setState(() {
        _networks = results;
        _status = 'Found ${results.length} networks';
      });
    } on PlatformException catch (e) {
      setState(() => _status = 'Scan error: ${e.message}');
    } finally {
      setState(() => _scanning = false);
    }
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
          ElevatedButton(
            onPressed: (_hasDiscovery == true && !_scanning) ? _scan : null,
            child: _scanning
                ? const SizedBox(
                    width: 20,
                    height: 20,
                    child: CircularProgressIndicator(strokeWidth: 2),
                  )
                : const Text('Scan'),
          ),
          if (_status != null) ...[
            const SizedBox(height: 8),
            Text(_status!, style: const TextStyle(fontStyle: FontStyle.italic)),
          ],
          const SizedBox(height: 16),
          ..._networks.map((n) => Card(
                child: ListTile(
                  title: Text(n.ssid.isEmpty ? '(hidden)' : n.ssid),
                  subtitle: Text('${n.bssid ?? ''} · ${n.security.name} · ${n.signalDbm ?? '?'} dBm'),
                ),
              )),
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
