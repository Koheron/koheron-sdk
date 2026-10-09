# Shared browser-test dependency resolution; source from a test runner.
web_test_repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$web_test_repo"
web_test_deps=tmp/tests/web-deps
export NODE_PATH=${NODE_PATH:-"$web_test_repo/web/node_modules:$web_test_repo/$web_test_deps/node_modules:/opt/app/node_modules"}
if ! node -e 'require.resolve("jsdom"); require.resolve("esbuild")' >/dev/null 2>&1; then
    mkdir -p "$web_test_deps"
    cp web/package.json web/package-lock.json "$web_test_deps/"
    npm ci --prefix "$web_test_deps"
    export NODE_PATH="$web_test_repo/$web_test_deps/node_modules:$NODE_PATH"
fi
