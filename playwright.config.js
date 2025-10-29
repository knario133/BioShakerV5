// playwright.config.js
module.exports = {
  testDir: 'tests',
  use: {
    headless: true,
    viewport: { width: 800, height: 600 },
    screenshot: 'only-on-failure',
  },
  webServer: {
    command: 'npx http-server data -p 8080',
    port: 8080,
    timeout: 120 * 1000,
    reuseExistingServer: !process.env.CI,
  },
};
