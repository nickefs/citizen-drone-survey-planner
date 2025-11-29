# 1) Πάρε ένα έτοιμο image με Node πάνω σε Debian/Ubuntu
FROM node:20-bullseye

# 2) Εγκατάστησε g++ για να κάνουμε build το C++ πρόγραμμα
RUN apt-get update && apt-get install -y g++ && rm -rf /var/lib/apt/lists/*

# 3) Όρισε φάκελο εργασίας μέσα στο container
WORKDIR /app

# 4) Αντέγραψε τα package*.json πρώτα και εγκατέστησε Node dependencies
COPY package*.json ./
RUN npm install

# 5) Αντέγραψε ΟΛΑ τα υπόλοιπα αρχεία του project στο /app
COPY . .

# 6) Κάνε compile το survey_cli από τα C++ αρχεία
RUN g++ -O2 -std=c++17 nqt_survey.cpp survey_cli.cpp -o survey_cli

# 7) Δήλωσε ότι ο server ακούει στη θύρα 3000
EXPOSE 3000

# 8) Πες στο container τι εντολή να τρέξει όταν ξεκινήσει
CMD ["node", "server.js"]
