# AssertJ Java6Assertions Migration Guide

The `Java6Assertions` class has been completely removed in modern versions of AssertJ.

### Instructions:
1. Replace all usages of `org.assertj.core.api.Java6Assertions` with `org.assertj.core.api.Assertions`.
2. Do not change the actual assertion logic or method chains (e.g., `assertThat().isEqualTo()`). The methods remain identical.

### Example BEFORE:
    import org.assertj.core.api.Java6Assertions;

    public void test() {
        Java6Assertions.assertThat(value).isNotNull();
    }

### Example AFTER:
    import org.assertj.core.api.Assertions;

    public void test() {
        Assertions.assertThat(value).isNotNull();
    }