pipeline_sim: pipeline_sim.cpp
	g++ -O2 -std=c++17 -Wall -o pipeline_sim pipeline_sim.cpp
clean:
	rm -f pipeline_sim
