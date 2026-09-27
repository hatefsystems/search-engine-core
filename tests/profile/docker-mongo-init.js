// Only the isolated verification database uses these fixed test credentials.
// Legacy C++ storage tests explicitly authenticate as this user.
db.getSiblingDB('admin').createUser({
  user: 'admin',
  pwd: 'password123',
  roles: [{role: 'root', db: 'admin'}],
});
