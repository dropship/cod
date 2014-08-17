#!/bin/env ruby
require 'socket'

class DropSim
  STATES = {
    ambient: 0,
    build: 1,
    drop_zone: 2,
    pre_drop: 3,
    drop: 4
  }

  EVENTS = [
    :kick, :snare, :chord, :wobble, :siren, :nop
  ]

  def initialize(ip, port, clock_val)
    @socket = UDPSocket.new
    @socket.connect(ip, port)
    @clock = clock_val
    build 0.0
    value 1.0
    crank :left, false
    crank :right, false
    state :ambient
  end

  def drop(&block)
    instance_eval &block
  end

  def state(state)
    @drop_state = STATES[state]
  end

  def crank(side, state)
    case side
    when :left
      @lcrank = state ? 1 : 0
    when :right
      @rcrank = state ? 1 : 0
    end
  end

  def build(amount)
    @build = amount
  end

  def value(num)
    @value = num
  end

  def events(*names)
    names.each do |name|
      if name.is_a?(Array)
        name.each { |n| @socket.send build_message(n), 0 }
      else
        @socket.send build_message(name), 0
      end
      sleep @clock
    end
  end


  private

  def build_message(name)
    case name
    when :chord, :wobble
      value rand * 0.3
    else
      value 1.0
    end

    raise "invalid event: #{name}" unless EVENTS.include?(name)
    [
      name.to_s,
      @value,
      @drop_state,
      @build,
      @lcrank,
      @rcrank
    ].join(',') + '$'
  end
end

s = DropSim.new "10.0.1.130", 9000, 1.0

s.drop do
  # AMBIENT
  state :ambient
  events :kick, :snare, [:kick, :chord], [:snare, :chord]

  # WHITE
  events :nop, [:nop, :chord]

  # PRE DROP (events not drawn)
  state :pre_drop
  events :kick, :snare, :kick, :snare

  # DROP (only wobble draws)
  state :drop
  events :kick, :snare, :kick, :snare
  events :wobble, :snare, :kick, :snare

  # AMBIENT
  state :ambient

  # CRANK (ignored by COD)
  crank :left, true
  crank :right, true
  events :kick, :snare, :kick, :snare

  # BUILD (ignored by COD)
  state :build
  build 0.1
  events :kick, :snare
  build 0.3
  events :kick, :snare
  build 0.5
  events :kick, :snare
  build 0.9
  events :kick, :snare
end
