# Javax to Jakarta EE Migration Guide

The Java EE project was moved to the Eclipse Foundation and renamed to Jakarta EE. As a result, the `javax.*` namespace has transitioned to `jakarta.*`.

### Instructions:
1. Update any fully-qualified class names in the code (e.g., `javax.servlet.http.HttpServletRequest` -> `jakarta.servlet.http.HttpServletRequest`).
2. Update annotations (e.g., `@javax.annotation.PostConstruct` -> `@jakarta.annotation.PostConstruct`).
3. CRITICAL: Do NOT change any internal business logic, method signatures, or variable names. The underlying APIs behave identically; this is strictly a namespace rename.

### Example BEFORE:
    public class MyServlet extends javax.servlet.http.HttpServlet {
        @javax.annotation.PostConstruct
        public void init() { ... }
    }

### Example AFTER:
    public class MyServlet extends jakarta.servlet.http.HttpServlet {
        @jakarta.annotation.PostConstruct
        public void init() { ... }
    }