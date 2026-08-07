FROM node:20-bullseye

# C++ compiler for the waypoint generator.
RUN apt-get update \
    && apt-get install -y --no-install-recommends g++ \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Install Node dependencies first so Docker can cache this layer.
COPY package*.json ./
RUN npm install --omit=dev

# Copy application source.
COPY . .

# Build the C++ survey CLI.
RUN g++ -O2 -std=c++17 \
    cpp/nqt_survey.cpp \
    cpp/survey_cli.cpp \
    -Icpp \
    -o survey_cli

EXPOSE 3000

CMD ["node", "server.js"]
