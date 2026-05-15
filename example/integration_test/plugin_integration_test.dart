import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';

import 'package:layrz_wifi/layrz_wifi.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('hasDiscovery and hasCurrentSsid return bools', (WidgetTester tester) async {
    final wifi = LayrzWifi.instance;
    final disc = await wifi.hasDiscovery();
    final ssid = await wifi.hasCurrentSsid();
    expect(disc, isA<bool>());
    expect(ssid, isA<bool>());
  });
}
