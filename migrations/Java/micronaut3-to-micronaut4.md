# Micronaut 3 to Micronaut 4 Migration Guide

You are migrating Java/Kotlin code from Micronaut 3 to Micronaut 4.

### Critical API Changes:
1. Jakarta EE Migration: Micronaut 4 upgrades to Jakarta EE 10. You MUST replace all `javax.inject`, `javax.annotation`, `javax.validation`, and `javax.transaction` imports with their `jakarta.*` equivalents.
2. Annotations: Core annotations like `@Singleton`, `@Inject`, `@PostConstruct`, and `@PreDestroy` now belong to the `jakarta` namespace.
3. Validation: Constraints like `@NotBlank`, `@NotNull`, and `@Min` have moved from `javax.validation.constraints` to `jakarta.validation.constraints`.
4. Reactive HTTP Clients: `RxHttpClient` and `RxStreamingHttpClient` (RxJava 2) have been completely removed. Replace them with the standard `HttpClient` and use Project Reactor (`Mono`/`Flux`) or RxJava 3.
5. Serialization: For JSON serialization, favor `io.micronaut.serde.annotation.Serdeable` over the older Jackson annotations or `@Introspected` if restructuring DTOs.

### Example BEFORE (Micronaut 3):
    import javax.inject.Inject;
    import javax.inject.Singleton;
    import javax.annotation.PostConstruct;
    import javax.validation.constraints.NotBlank;
    import io.micronaut.http.client.RxHttpClient;
    import io.reactivex.Single;

    @Singleton
    public class LegacyService {
        
        @Inject
        private RxHttpClient httpClient;

        @PostConstruct
        public void init() {
            // initialization
        }

        public Single<String> fetchData(@NotBlank String query) {
            return httpClient.retrieve("/api/" + query).firstOrError();
        }
    }

### Example AFTER (Micronaut 4):
    import jakarta.inject.Inject;
    import jakarta.inject.Singleton;
    import jakarta.annotation.PostConstruct;
    import jakarta.validation.constraints.NotBlank;
    import io.micronaut.http.client.HttpClient;
    import reactor.core.publisher.Mono;

    @Singleton
    public class LegacyService {
        
        @Inject
        private HttpClient httpClient;

        @PostConstruct
        public void init() {
            // initialization
        }

        public Mono<String> fetchData(@NotBlank String query) {
            return Mono.from(httpClient.retrieve("/api/" + query));
        }
    }