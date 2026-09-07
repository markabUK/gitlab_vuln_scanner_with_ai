# Mockito Legacy to Mockito Core Migration Guide

You are updating legacy Mockito code to modern Mockito 3.x/4.x.

### Critical API Changes:
1. Matchers Class: The `org.mockito.Matchers` class is deprecated/removed. Replace all calls to it with `org.mockito.ArgumentMatchers`.
2. Any Object: Replace `anyObject()` and `anyVararg()` with `any()`.
3. Any Class Type: Replace `any(SomeClass.class)` with `any(SomeClass.class)` (remains same) or `isA(SomeClass.class)`.
4. JUnit 5 Integration: If migrating to JUnit 5 alongside Mockito, replace `@RunWith(MockitoJUnitRunner.class)` with `@ExtendWith(MockitoExtension.class)`.

### Example BEFORE:
    @RunWith(MockitoJUnitRunner.class)
    public class MyTest {
        @Test
        public void test() {
            when(myMock.doSomething(Matchers.anyObject())).thenReturn(true);
            when(myMock.doSomethingElse(Matchers.anyString())).thenReturn(false);
        }
    }

### Example AFTER:
    @ExtendWith(MockitoExtension.class) // If using JUnit 5
    public class MyTest {
        @Test
        public void test() {
            when(myMock.doSomething(ArgumentMatchers.any())).thenReturn(true);
            when(myMock.doSomethingElse(ArgumentMatchers.anyString())).thenReturn(false);
        }
    }