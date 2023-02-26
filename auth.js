document.addEventListener("DOMContentLoaded", function(){
    // listen for auth status changes
    auth.onAuthStateChanged(user => {
      if (user) {
        console.log("utente autenticato");
        console.log(user);
        setupUI(user);
        var uid = user.uid;
        console.log(uid);
      } else {
        console.log("utente non autenticato");
        setupUI();
      }
    });
  
    // login
    const loginForm = document.querySelector('#login-form');
    loginForm.addEventListener('submit', (e) => {
      e.preventDefault();
      // get user info
      const email = loginForm['input-email'].value;
      const password = loginForm['input-password'].value;
      // log the user in
      auth.signInWithEmailAndPassword(email, password).then((cred) => {
        // close the login modal & reset form
        $('#login-modal').modal("hide");
        loginForm.reset();
      });
    });
  
    // logout
    const logout = document.querySelector('#logout');
    logout.addEventListener('click', (e) => {
      e.preventDefault();
      auth.signOut();
    });
  });
