/* Flot resize plugin, derived from Flot 0.8.3.
 * Copyright (c) 2007-2014 IOLA and Ole Laursen. Licensed under MIT.
 * Koheron: observe the plot element instead of polling every jQuery resize
 * subscriber. Coalesce callbacks and release observers on plot shutdown.
 */
(function ($) {
    function init(plot) {
        var observer, fallbackTimer, frame = null, element, lastWidth, lastHeight;
        function onResize() {
            frame = null;
            var placeholder = plot.getPlaceholder();
            var width = placeholder.width(), height = placeholder.height();
            if (!width || !height || (width === lastWidth && height === lastHeight)) return;
            lastWidth = width; lastHeight = height;
            plot.resize(); plot.setupGrid(); plot.draw();
        }
        function schedule() {
            if (frame == null) frame = window.requestAnimationFrame(onResize);
        }
        function bindEvents() {
            element = plot.getPlaceholder()[0];
            lastWidth = plot.getPlaceholder().width();
            lastHeight = plot.getPlaceholder().height();
            // Preserve explicitly triggered resize events used by clients.
            element.addEventListener('resize', schedule);
            plot.getPlaceholder().on('resize', schedule);
            if (window.ResizeObserver) {
                observer = new ResizeObserver(schedule);
                observer.observe(element);
            } else {
                // Older browsers retain element-layout detection, including
                // sidebar changes, without the global jQuery resize registry.
                window.addEventListener('resize', schedule);
                fallbackTimer = window.setInterval(schedule, 200);
            }
        }
        function shutdown() {
            if (observer) observer.disconnect();
            if (fallbackTimer != null) window.clearInterval(fallbackTimer);
            window.removeEventListener('resize', schedule);
            if (element) element.removeEventListener('resize', schedule);
            plot.getPlaceholder().off('resize', schedule);
            if (frame != null) window.cancelAnimationFrame(frame);
            frame = null;
        }
        plot.hooks.bindEvents.push(bindEvents);
        plot.hooks.shutdown.push(shutdown);
    }
    $.plot.plugins.push({init: init, options: {}, name: 'resize', version: '1.0-koheron'});
})(jQuery);
