# Micronaut 4 to Micronaut 5 Migration Guide

You are migrating Java/Kotlin code from Micronaut 4 to Micronaut 5.1.x.

### Critical API Changes:
1. Nullability Annotations: Micronaut 5 formally adopts JSpecify. Replace `javax.annotation.Nullable`, `jakarta.annotation.Nullable`, or `io.micronaut.core.annotation.Nullable` with `org.jspecify.annotations.Nullable` (and similarly for `@NonNull`).
2. Reactive Streams: RxJava 2 is no longer supported. You must migrate RxJava 2 imports (`io.reactivex.*`) to RxJava 3 (`io.reactivex.rxjava3.core.*`) or Project Reactor (`reactor.core.publisher.*`). Replace `Single<T>` with `Mono<T>` or `io.reactivex.rxjava3.core.Single<T>`.
3. Micronaut Views: The `@TurboView` annotation has been renamed to `@TurboStreamView`.
4. Embedded Data: If using embedded fields in Micronaut Data, the embedded naming strategy changed. Ensure fields inside `@Embeddable` classes are correctly mapped if they previously relied on legacy implicit naming.

### Example BEFORE:
    import io.micronaut.core.annotation.Nullable;
    import io.micronaut.views.turbo.TurboView;
    import io.reactivex.Single;
    import io.micronaut.http.annotation.Get;
    import io.micronaut.http.annotation.Controller;

    @Controller("/api")
    public class LegacyController {

        @Get("/view")
        @TurboView("my-view")
        public Single<String> renderView(@Nullable String name) {
            return Single.just(name == null ? "Default" : name);
        }
    }

### Example AFTER:
    import org.jspecify.annotations.Nullable;
    import io.micronaut.views.turbo.TurboStreamView;
    import reactor.core.publisher.Mono;
    import io.micronaut.http.annotation.Get;
    import io.micronaut.http.annotation.Controller;

    @Controller("/api")
    public class LegacyController {

        @Get("/view")
        @TurboStreamView("my-view")
        public Mono<String> renderView(@Nullable String name) {
            return Mono.just(name == null ? "Default" : name);
        }
    }