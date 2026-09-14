# JUnit 5 to JUnit 6 Migration Guide

JUnit 6 consolidates the platform and introduces modern language baselines. While the core `org.junit.jupiter.api` imports remain identical, several build-level and runtime changes must be addressed.

### Critical API and Structural Changes:
1. **Java 17 & Kotlin 2.2 Baseline**: JUnit 6 mandates Java 17+ and Kotlin 2.2+. If the project is on Java 11 or older, you cannot upgrade the dependencies.
2. **Native Kotlin Coroutines**: Kotlin `suspend` functions are now natively supported for `@Test` and lifecycle methods. You MUST remove `runBlocking` wrappers from your test functions.
3. **Consolidated Dependencies**: `junit-platform-runner`, `junit-platform-jfr`, and `junit-platform-suite-commons` have been removed. Their contents are now directly included in `junit-platform-launcher`. Remove these legacy explicit dependencies from the build file.
4. **Vintage Engine Deprecation**: `junit-vintage-engine` (used to run legacy JUnit 4 tests) is now formally deprecated.
5. **CSV Parsing Engine**: `@CsvSource` and `@CsvFileSource` now use `FastCSV` instead of `univocity-parsers`. Verify that any malformed or edge-case CSV formatting in parameterized tests still parses correctly.
6. **Nullability**: All JUnit modules now use `JSpecify` nullability annotations.

### Example BEFORE (Kotlin Coroutines in JUnit 5):
    import org.junit.jupiter.api.Test;
    import kotlinx.coroutines.runBlocking;
    import kotlinx.coroutines.delay;

    public class ApiTest {
        @Test
        fun testApiCall() = runBlocking {
            delay(1000)
            assertEquals(200, response.status)
        }
    }

### Example AFTER (Kotlin Coroutines in JUnit 6):
    import org.junit.jupiter.api.Test;
    import kotlinx.coroutines.delay;

    public class ApiTest {
        // runBlocking is no longer needed, use suspend directly
        @Test
        suspend fun testApiCall() {
            delay(1000)
            assertEquals(200, response.status)
        }
    }