function themeToggle() {
  let themeSwitch = document.getElementById("checkbox-id");
  // check if the checkbox exist
  if (themeSwitch) {
    // get the user saved theme
    let savedTheme = localStorage.getItem("user-theme");

    if (savedTheme === "dark")
      document.getElementById("checkbox-id").checked = true;
    else document.getElementById("checkbox-id").checked = false;

    // save user theme choice
    themeSwitch.addEventListener("change", () => {
      if (themeSwitch.checked)
        savedTheme = localStorage.setItem("user-theme", "dark");
      else savedTheme = localStorage.setItem("user-theme", "light");
    });
  } else {
    console.error("Error in theme switch");
  }
}
// wait until DOM loads
document.addEventListener("DOMContentLoaded", function () {
  themeToggle();
});
