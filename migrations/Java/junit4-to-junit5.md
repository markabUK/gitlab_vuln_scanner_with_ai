# JUnit 4 to JUnit 5 (Jupiter) Migration Guide

You are migrating tests from JUnit 4 to JUnit 5 (JUnit Jupiter). You MUST completely remove all `org.junit` and `org.junit.Assert` imports and replace them with `org.junit.jupiter.api.*`.

### Critical API and Structural Changes:
1. Assertion Messages: In JUnit 4, the message string is the FIRST argument. In JUnit 5, it is the LAST argument. You MUST swap the parameter order for `assertEquals`, `assertTrue`, `assertNotNull`, etc.
2. Expected Exceptions: The `@Test(expected = Exception.class)` parameter is removed. Wrap the code in `Assertions.assertThrows(Exception.class, () -> { ... })`.
3. Timeouts: The `@Test(timeout = 100)` parameter is removed. Use the `@Timeout` annotation.
4. Lifecycle Annotations: 
   - `@Before` -> `@BeforeEach`
   - `@After` -> `@AfterEach`
   - `@BeforeClass` -> `@BeforeAll`
   - `@AfterClass` -> `@AfterAll`
5. Disabling Tests: `@Ignore` -> `@Disabled`
6. Runners: `@RunWith` is completely removed. Replace `@RunWith(MockitoJUnitRunner.class)` with `@ExtendWith(MockitoExtension.class)`, and `@RunWith(SpringRunner.class)` with `@ExtendWith(SpringExtension.class)`.
7. Rules: The `@Rule` and `@ClassRule` annotations are removed.
   - For Mockito, switch to `@ExtendWith(MockitoExtension.class)`.
   - For temporary folders, replace `@Rule public TemporaryFolder folder` with the `@TempDir` annotation on a Path or File parameter/field.
   - For custom rules, they must be rewritten to implement JUnit 5 `Extension` interfaces and registered via `@RegisterExtension`.
8. Categories: `@Category(MyIntegrationTest.class)` -> `@Tag("MyIntegrationTest")`.
9. Assumptions: `org.junit.Assume.*` -> `org.junit.jupiter.api.Assumptions.*`.

### Example BEFORE (JUnit 4):
    import org.junit.Test;
    import org.junit.Before;
    import org.junit.Rule;
    import org.junit.Ignore;
    import org.junit.Assert;
    import org.junit.runner.RunWith;
    import org.junit.rules.TemporaryFolder;
    import org.mockito.junit.MockitoJUnitRunner;

    @RunWith(MockitoJUnitRunner.class)
    public class MyTest {

        @Rule
        public TemporaryFolder tempFolder = new TemporaryFolder();

        @Before
        public void setUp() { }

        @Test(expected = IllegalArgumentException.class)
        public void testException() {
            calculator.divide(1, 0);
        }

        @Test
        public void testMessage() {
            Assert.assertEquals("Values should match", 5, result);
        }
    }

### Example AFTER (JUnit 5):
    import org.junit.jupiter.api.Test;
    import org.junit.jupiter.api.BeforeEach;
    import org.junit.jupiter.api.Disabled;
    import org.junit.jupiter.api.Assertions;
    import org.junit.jupiter.api.extension.ExtendWith;
    import org.junit.jupiter.api.io.TempDir;
    import org.mockito.junit.jupiter.MockitoExtension;
    import java.nio.file.Path;

    @ExtendWith(MockitoExtension.class)
    public class MyTest {

        @TempDir
        Path tempFolder;

        @BeforeEach
        public void setUp() { }

        @Test
        public void testException() {
            Assertions.assertThrows(IllegalArgumentException.class, () -> {
                calculator.divide(1, 0);
            });
        }

        @Test
        public void testMessage() {
            // Note how the message string moved to the end!
            Assertions.assertEquals(5, result, "Values should match");
        }
    }