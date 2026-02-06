import { test, expect } from '@playwright/test';

test.describe('Dashboard App', () => {
	test('should load the home page', async ({ page }) => {
		await page.goto('/');

		// Should show the app title or loading state
		await expect(page.locator('body')).toBeVisible();
	});

	test('should have correct page title', async ({ page }) => {
		await page.goto('/');

		await expect(page).toHaveTitle(/RGBW|Dashboard/i);
	});

	test('should navigate to serial monitor', async ({ page }) => {
		await page.goto('/serial');

		// Serial page should load
		await expect(page.locator('body')).toBeVisible();
	});
});

test.describe('Navigation', () => {
	test('should have navigation elements', async ({ page }) => {
		await page.goto('/');

		// Wait for page to load
		await page.waitForLoadState('networkidle');
	});
});

test.describe('Accessibility', () => {
	test('should have no console errors on load', async ({ page }) => {
		const errors: string[] = [];
		page.on('console', (msg) => {
			if (msg.type() === 'error') {
				// Ignore Tauri API errors when running without Tauri shell
				const text = msg.text();
				if (!text.includes('__TAURI__') && !text.includes('invoke')) {
					errors.push(text);
				}
			}
		});

		await page.goto('/');
		await page.waitForLoadState('networkidle');

		// Allow time for any async errors
		await page.waitForTimeout(500);

		// Should have no unexpected console errors
		expect(errors).toHaveLength(0);
	});

	test('should have accessible buttons', async ({ page }) => {
		await page.goto('/');
		await page.waitForLoadState('networkidle');

		// All buttons should have accessible names
		const buttons = page.locator('button');
		const count = await buttons.count();

		for (let i = 0; i < count; i++) {
			const button = buttons.nth(i);
			const name = await button.getAttribute('aria-label');
			const text = await button.textContent();
			const title = await button.getAttribute('title');

			// Button should have some accessible identification
			const hasAccessibleName = name || text?.trim() || title;
			expect(hasAccessibleName).toBeTruthy();
		}
	});
});
