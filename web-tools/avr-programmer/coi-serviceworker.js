/*! coi-serviceworker v0.1.7 compatible implementation
 * Based on https://github.com/gzuidhof/coi-serviceworker
 * Copyright Guido Zuidhof and contributors, MIT License.
 */
let coepCredentialless = false;

if (typeof window === "undefined") {
  self.addEventListener("install", () => self.skipWaiting());
  self.addEventListener("activate", (event) => event.waitUntil(self.clients.claim()));

  self.addEventListener("message", (event) => {
    if (!event.data) {
      return;
    }

    if (event.data.type === "deregister") {
      self.registration
        .unregister()
        .then(() => self.clients.matchAll())
        .then((clients) => clients.forEach((client) => client.navigate(client.url)));
      return;
    }

    if (event.data.type === "coepCredentialless") {
      coepCredentialless = Boolean(event.data.value);
    }
  });

  self.addEventListener("fetch", (event) => {
    const request = event.request;
    if (request.cache === "only-if-cached" && request.mode !== "same-origin") {
      return;
    }

    const isolatedRequest = coepCredentialless && request.mode === "no-cors"
      ? new Request(request, { credentials: "omit" })
      : request;

    event.respondWith(
      fetch(isolatedRequest)
        .then((response) => {
          if (response.status === 0) {
            return response;
          }

          const headers = new Headers(response.headers);
          headers.set(
            "Cross-Origin-Embedder-Policy",
            coepCredentialless ? "credentialless" : "require-corp",
          );
          headers.set("Cross-Origin-Opener-Policy", "same-origin");

          if (!coepCredentialless) {
            headers.set("Cross-Origin-Resource-Policy", "cross-origin");
          }

          return new Response(response.body, {
            status: response.status,
            statusText: response.statusText,
            headers,
          });
        })
        .catch((error) => {
          console.error("COI service worker fetch failed:", error);
          return Response.error();
        }),
    );
  });
} else {
  (() => {
    const reloadedBySelf = window.sessionStorage.getItem("coiReloadedBySelf");
    window.sessionStorage.removeItem("coiReloadedBySelf");

    const coepDegrading = reloadedBySelf === "coepdegrade";
    const config = {
      shouldRegister: () => !reloadedBySelf,
      shouldDeregister: () => false,
      coepCredentialless: () => true,
      coepDegrade: () => true,
      doReload: () => window.location.reload(),
      quiet: false,
      ...(window.coi ?? {}),
    };

    const serviceWorker = navigator.serviceWorker;
    const controlling = Boolean(serviceWorker?.controller);

    if (controlling && !window.crossOriginIsolated) {
      window.sessionStorage.setItem("coiCoepHasFailed", "true");
    }

    const coepHasFailed = window.sessionStorage.getItem("coiCoepHasFailed");

    if (controlling) {
      const reloadToDegrade = config.coepDegrade()
        && !(coepDegrading || window.crossOriginIsolated);

      serviceWorker.controller.postMessage({
        type: "coepCredentialless",
        value: (reloadToDegrade || (coepHasFailed && config.coepDegrade()))
          ? false
          : config.coepCredentialless(),
      });

      if (reloadToDegrade) {
        if (!config.quiet) {
          console.log("Reloading page to degrade COEP to require-corp.");
        }
        window.sessionStorage.setItem("coiReloadedBySelf", "coepdegrade");
        config.doReload("coepdegrade");
        return;
      }

      if (config.shouldDeregister()) {
        serviceWorker.controller.postMessage({ type: "deregister" });
      }
    }

    if (window.crossOriginIsolated !== false || !config.shouldRegister()) {
      return;
    }

    if (!window.isSecureContext) {
      if (!config.quiet) {
        console.error("COOP/COEP service worker requires HTTPS or localhost.");
      }
      return;
    }

    if (!serviceWorker) {
      if (!config.quiet) {
        console.error("COOP/COEP service worker is unavailable in this browser context.");
      }
      return;
    }

    serviceWorker.register(window.document.currentScript.src).then(
      (registration) => {
        if (!config.quiet) {
          console.log("COOP/COEP service worker registered:", registration.scope);
        }

        registration.addEventListener("updatefound", () => {
          if (!config.quiet) {
            console.log("Reloading to activate the updated COOP/COEP service worker.");
          }
          window.sessionStorage.setItem("coiReloadedBySelf", "updatefound");
          config.doReload("updatefound");
        });

        if (registration.active && !serviceWorker.controller) {
          if (!config.quiet) {
            console.log("Reloading to activate cross-origin isolation.");
          }
          window.sessionStorage.setItem("coiReloadedBySelf", "notcontrolling");
          config.doReload("notcontrolling");
        }
      },
      (error) => {
        if (!config.quiet) {
          console.error("COOP/COEP service worker registration failed:", error);
        }
      },
    );
  })();
}
