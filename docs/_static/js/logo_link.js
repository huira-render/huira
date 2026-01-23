// Make logo link to main website (Furo theme)
document.addEventListener('DOMContentLoaded', function() {
    var logoLink = document.querySelector('.sidebar-brand');
    if (logoLink) {
        logoLink.href = 'https://www.huira.space';
        logoLink.target = '_blank';
        logoLink.title = 'Visit huira.space';
    }
});
