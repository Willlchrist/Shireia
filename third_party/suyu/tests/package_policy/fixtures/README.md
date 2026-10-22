# Synthetic OpenPGP syntax fixture

shireia-synthetic-detached-signature.asc is a minimal v4 binary document-signature packet wrapped in ASCII armor. It was generated specifically for parser tests in this Shireia public snapshot. Its RSA MPI is a one-bit placeholder; it is not cryptographically valid, is not signed by any key, and is not associated with a Debian source archive.

The tests use it to check bounded packet parsing, manifest SHA-256/authentication metadata handling, malformed/trailing-payload rejection, and collector receipt behavior. Authentication is never inferred from this fixture. The test implementation and fixture are distributed with the applicable Suyu source/test license notices preserved.
