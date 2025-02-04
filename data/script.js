(function($) {

  $.fn.menumaker = function(options) {
      
      var cssmenu = $(this), settings = $.extend({
        title: "Menu",
        format: "dropdown",
        sticky: false
      }, options);

      return this.each(function() {
        cssmenu.prepend('<div id="menu-button" class="menu-button">' + settings.title + '</div>');
        $(this).find("#menu-button").on('click', function(){
          $(this).toggleClass('menu-opened');
          var mainmenu = $(this).next('ul');
          if (mainmenu.hasClass('open')) { 
            mainmenu.hide().removeClass('open');
          }
          else {
            mainmenu.show().addClass('open');
            if (settings.format === "dropdown") {
              mainmenu.find('ul').show();
            }
          }
        });

        cssmenu.find('li ul').parent().addClass('has-sub');

        multiTg = function() {
          cssmenu.find(".has-sub").prepend('<span class="submenu-button"></span>');
          cssmenu.find('.submenu-button').on('click', function() {
            $(this).toggleClass('submenu-opened');
            if ($(this).siblings('ul').hasClass('open')) {
              $(this).siblings('ul').removeClass('open').hide();
            }
            else {
              $(this).siblings('ul').addClass('open').show();
            }
          });
        };

        if (settings.format === 'multitoggle') multiTg();
        else cssmenu.addClass('dropdown');

        if (settings.sticky === true) cssmenu.css('position', 'fixed');

        // Ensure menu button always appears at the right
        cssmenu.css('position', 'relative');
        cssmenu.find("#menu-button").css({
          'position': 'absolute',
          'right': '-37px',
          'top': '0px',
          'cursor': 'pointer',
          'z-index': '9999', // Ensure button is above other elements
          'background': '#333', // Match background color
          'color': '#ffffff', // Set text color
          'padding': '10px',
          'border-radius': '10px',
          'transition': 'color-background 0.3s',
          'box-shadow': '0 0.4rem #dfd9d9',

        });

        // Always show the menu button and keep menu items hidden initially
        cssmenu.find('ul').hide();
        cssmenu.find("#menu-button").show();
      });
  };
})(jQuery);

(function($){
$(document).ready(function(){

$("#cssmenu").menumaker({
   title: " ☰ ", // Use hamburger icon for the button
   format: "multitoggle"
});

});
})(jQuery);
