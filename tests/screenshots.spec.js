// tests/screenshots.spec.js
const { test, expect } = require('@playwright/test');

test.describe('BioShaker Web UI Screenshots', () => {
  test('Connected State (STA)', async ({ page }) => {
    await page.route('/status', route => {
      route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({
          wifi: true,
          mode: 'STA',
          ip: '192.168.1.123',
          ssid: 'MyWiFi',
          rssi: -55,
          currentRpm: 250.5,
        }),
      });
    });
    await page.goto('http://localhost:8080');
    await page.waitForTimeout(1500); // Wait for the status update
    await page.screenshot({ path: 'docs/screenshots/web-ui-connected.png' });
  });

  test('Access Point Mode (AP)', async ({ page }) => {
    await page.route('/status', route => {
      route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({
          wifi: false,
          mode: 'AP',
          ip_ap: '192.168.4.1',
          ssid: '',
          rssi: null,
          currentRpm: 0.0,
        }),
      });
    });
    await page.goto('http://localhost:8080');
    await page.waitForTimeout(2000); // Wait for the blinking text
    await page.screenshot({ path: 'docs/screenshots/web-ui-ap-mode.png' });
  });

  test('Disconnected State', async ({ page }) => {
    await page.route('/status', route => {
      route.fulfill({
        status: 500,
      });
    });
    await page.goto('http://localhost:8080');
    await page.waitForTimeout(1500); // Wait for the error message
    await page.screenshot({ path: 'docs/screenshots/web-ui-disconnected.png' });
  });
});
