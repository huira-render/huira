// Make logo link to main website
document.addEventListener('DOMContentLoaded', function() {
    var logoLink = document.querySelector('.wy-side-nav-search > a');
    if (logoLink) {
        logoLink.href = 'https://www.huira.space';
        logoLink.target = '_blank';
        logoLink.title = 'Visit huira.space';
    }
});
