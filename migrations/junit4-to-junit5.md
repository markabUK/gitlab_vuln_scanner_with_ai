# JUnit 4 to JUnit 5 (Jupiter) Migration Guide

You are migrating tests from JUnit 4 to JUnit 5. 

### Critical Structural Changes:
1. Assertion Messages: In JUnit 4, the message is the FIRST argument. In JUnit 5, it is the LAST argument. You MUST swap the order.
2. Expected Exceptions: The `@Test(expected = Exception.class)` parameter is removed. You MUST wrap the throwing code in `Assertions.assertThrows()`.
3. Timeouts: The `@Test(timeout = 100)` parameter is removed. Use the `@Timeout` annotation.

### Example BEFORE (JUnit 4):
    @Test(expected = IllegalArgumentException.class)
    public void testException() {
        calculator.divide(1, 0);
    }

    @Test(timeout = 1000)
    public void testTimeout() {
        calculator.heavyCalculation();
    }

    @Test
    public void testMessage() {
        Assert.assertEquals("Values should match", 5, result);
        Assert.assertTrue("Should be true", result > 0);
    }

### Example AFTER (JUnit 5):
    @Test
    public void testException() {
        Assertions.assertThrows(IllegalArgumentException.class, () -> {
            calculator.divide(1, 0);
        });
    }

    @Test
    @Timeout(value = 1000, unit = TimeUnit.MILLISECONDS)
    public void testTimeout() {
        calculator.heavyCalculation();
    }

    @Test
    public void testMessage() {
        // Note how the message string moved to the end!
        Assertions.assertEquals(5, result, "Values should match");
        Assertions.assertTrue(result > 0, "Should be true");
    }