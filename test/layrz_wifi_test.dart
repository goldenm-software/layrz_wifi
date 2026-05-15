import 'package:flutter_test/flutter_test.dart';
import 'package:layrz_wifi/layrz_wifi.dart';

void main() {
  test('LayrzWifi.instance returns a singleton', () {
    expect(LayrzWifi.instance, same(LayrzWifi.instance));
  });
}
