# Shared browser-test dependency resolution; source from a test runner.
web_test_repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$web_test_repo"
web_test_deps=tmp/tests/web-deps
export NODE_PATH=${NODE_PATH:-"$web_test_repo/web/node_modules:$web_test_repo/$web_test_deps/node_modules:/opt/app/node_modules"}
if ! node -e 'require.resolve("jsdom"); require.resolve("typescript")' >/dev/null 2>&1; then
    npm install --prefix "$web_test_deps" --no-save --package-lock=false typescript@5.6.3 jsdom@26.1.0
    export NODE_PATH="$web_test_repo/$web_test_deps/node_modules:$NODE_PATH"
fi
